#define NOMINMAX
#define CROW_MAIN
#include "crow.h"
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>
#include <random>
#include <utility>
#include <iomanip>
#include <algorithm>
#include <cctype> 
#include <cmath>  

#include "backend.h"
using namespace std;
namespace fs = std::filesystem;
using App = crow::App<struct CORSMiddleware>;

unordered_map<string, time_t> admin_tokens;
unordered_map<string, pair<int, time_t>> user_tokens;
const int TOKEN_EXPIRY = 1800; // 30 minutes in seconds

struct CORSMiddleware 
{
    struct context {};
    void before_handle(crow::request& req, crow::response& res, context&) {
        if (req.method == "OPTIONS"_method) {
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.code = 204;
            res.end();
        }
    }
    void after_handle(crow::request&, crow::response& res, context&) 
    {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    }
};

string generateUserToken() 
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    string token;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    for (int i = 0; i < 32; ++i) token += charset[dist(gen)];
    return token;
}

crow::json::wvalue make_account_json(Account* acc) 
{
    crow::json::wvalue json;
    json["accNum"] = acc->getAccountNumber();
    json["accHolder"] = acc->getAccountHolder();
    json["balance"] = acc->getBalance();
    json["accType"] = acc->getAccountTypeName();
    json["isActive"] = acc->isAccountActive();
    // Added dummy data for frontend compatibility
    json["branch"] = "Main Branch";
    json["ifsc"] = "BANK001";
    // Generate creation date (using account creation time if available, else current time)
    time_t now = time(nullptr);
    struct tm tstruct;
#ifdef _WIN32
    localtime_s(&tstruct, &now);
#else
    localtime_r(&now, &tstruct);
#endif
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
    json["createdDate"] = string(buf);
    
    return json;
}

string read_file(const string& filename) 
{
    ifstream file(filename, ios::binary);
    if (!file) return "";
    ostringstream ss; ss << file.rdbuf(); return ss.str();
}

void logTransaction(int accNum, const string& type, double amount) 
{
    try {
        // Create logs directory if it doesn't exist
        fs::path logDir = fs::current_path() / "logs";
        if (!fs::exists(logDir)) 
        {
            fs::create_directories(logDir);
        }

        ofstream file(logDir / "transaction_log.txt", ios::app);
        if (!file.is_open()) 
        {
            cerr << " Cannot open transaction_log.txt in logs directory\n";
            return;
        }

        time_t now = time(nullptr);
        struct tm tstruct;
    #ifdef _WIN32
        localtime_s(&tstruct, &now);
    #else
        localtime_r(&now, &tstruct);
    #endif

        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
        file << accNum << " | " << type << " | " << amount << " | " << buf << "\n";
    } catch (...) {
        cerr << "Logging error occurred.\n";
    }
}

bool verifyUserToken(const string& token, int& accountNumber) {
    if (token.empty()) return false;
    
    auto user_it = user_tokens.find(token);
    if (user_it != user_tokens.end()) 
    {
        if (time(0) - user_it->second.second > TOKEN_EXPIRY) 
        {
            user_tokens.erase(user_it);
            return false;
        }
        
        accountNumber = user_it->second.first;
        user_it->second.second = time(0);
        return true;
    }
    
    auto admin_it = admin_tokens.find(token);
    if (admin_it != admin_tokens.end()) 
    {
        if (time(0) - admin_it->second > TOKEN_EXPIRY) 
        {
            admin_tokens.erase(admin_it);
            return false;
        }
        
        accountNumber = -1;
        admin_it->second = time(0);
        return true;
    }
    
    return false;
}

bool verifyAdminToken(const string& token) 
{
    auto it = admin_tokens.find(token);
    if (it == admin_tokens.end()) return false;
    
    if (time(0) - it->second > TOKEN_EXPIRY) 
    {
        admin_tokens.erase(it);
        return false;
    }
    
    it->second = time(0);
    return true;
}

template<typename Func>
crow::response handle_account_op(const crow::request& req, Func op) {
    try {
        auto auth_header = req.get_header_value("Authorization");
        if (auth_header.empty())
            return crow::response(401, "{\"error\":\"Missing token\"}");

        int accountNumber = -1;
        if (!verifyUserToken(auth_header, accountNumber)) {
            CROW_LOG_INFO << "Token verification failed for: " << auth_header;
            return crow::response(401, "{\"error\":\"Invalid or expired token\"}");
        }

        if (accountNumber == -1) {
            return crow::response(403, "{\"error\":\"Admin cannot perform account operations\"}");
        }

        Account* acc = Account::loadAccountData(accountNumber, "", true);
        if (!acc) return crow::response(404, "{\"error\":\"Account not found\"}");
        if (!acc->isAccountActive()) {
            delete acc;
            return crow::response(403, "{\"error\":\"Account inactive\"}");
        }

        auto result = op(acc);
        acc->saveAccountData();
        delete acc;
        return crow::response(200, result);
    } catch (const exception& e) {
        return crow::response(500, "{\"error\":\"" + string(e.what()) + "\"}");
    }
}

void cleanExpiredTokens() {
    time_t now = time(0);
    
    for (auto it = user_tokens.begin(); it != user_tokens.end(); ) {
        if (now - it->second.second > TOKEN_EXPIRY) {
            it = user_tokens.erase(it);
        } else {
            ++it;
        }
    }
    
    for (auto it = admin_tokens.begin(); it != admin_tokens.end(); ) {
        if (now - it->second > TOKEN_EXPIRY) {
            it = admin_tokens.erase(it);
        } else {
            ++it;
        }
    }
}

void tokenCleaner() {
    while (true) {
        this_thread::sleep_for(chrono::minutes(5));
        cleanExpiredTokens();
        CROW_LOG_INFO << "Token cleaner ran, current tokens: " 
                      << user_tokens.size() << " user, " 
                      << admin_tokens.size() << " admin";
    }
}

string generateAdminToken(const string& username) {
    string token = generateUserToken();
    admin_tokens[token] = time(0);
    return token;
}

string generateTokenForAccount(int accNum) {
    string token = generateUserToken();
    user_tokens[token] = {accNum, time(0)};
    return token;
}

int extractAccountNumberFromToken(const string& token) {
    int accNum = -1;
    if (verifyUserToken(token, accNum)) return accNum;
    return -1;
}
double calculateEMI(double principal, double rate, int months) {
    double monthlyRate = rate / (12 * 100);
    return (principal * monthlyRate * pow(1 + monthlyRate, months)) / (pow(1 + monthlyRate, months) - 1);
}

string getMimeType(const string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == string::npos) {
        return "application/octet-stream";
    }
    
    string ext = path.substr(dot_pos);
    transform(ext.begin(), ext.end(), ext.begin(), 
              [](unsigned char c){ return tolower(c); });
    
    if (ext == ".html") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    return "application/octet-stream";

  

}
int main(int argc, char* argv[]) {
    srand(static_cast<unsigned>(time(0)));

    fs::path exePath = fs::absolute(fs::path(argv[0])).parent_path();
    fs::current_path(exePath);  

    // Now paths like fs::current_path() / "accounts" will always point to build/bin/accounts

    // Create required directories with validation
    fs::path accPath = fs::current_path() / "accounts";
    if (!fs::exists(accPath)) {
        cerr << "accounts folder not found at: " << accPath << endl;
        cerr << "Creating accounts directory..." << endl;
        fs::create_directories(accPath);
    }

    fs::path frontendPath = fs::current_path() / "frontend";
    if (!fs::exists(frontendPath)) {
        cerr << "frontend directory not found at: " << frontendPath << endl;
        cerr << " Creating frontend directory..." << endl;
        fs::create_directories(frontendPath);
    }

    fs::path logsPath = fs::current_path() / "logs";
    if (!fs::exists(logsPath)) {
        cerr << "Creating logs directory..." << endl;
        fs::create_directories(logsPath);
    }




    thread cleaner(tokenCleaner);
    cleaner.detach();

    App app;

    // Root route
    CROW_ROUTE(app, "/")([] {
        string html = read_file("frontend/start.html");
        if (html.empty()) {
            crow::response res(404, "Start page not found");
            res.set_header("Content-Type", "text/plain");
            return res;
        }
        crow::response res(200, html);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // API Routes
    CROW_ROUTE(app, "/api/account/create").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "{\"error\":\"Invalid JSON\"}");

        int accNum = body["accountNumber"].i();
        if (Account::accountExists(accNum))
            return crow::response(409, "{\"error\":\"Account already exists\"}");

        Account* acc = nullptr;
        int type = body["accountType"].i();

        if (type == 1) {
            acc = new SavingsAccount(accNum, body["accountHolder"].s(), body["balance"].d(), body["password"].s());
        } else if (type == 2) {
            acc = new CurrentAccount(accNum, body["accountHolder"].s(), body["balance"].d(), body["password"].s());
        } else if (type == 3) {
            acc = new LoanAccount(accNum, body["accountHolder"].s(), body["balance"].d(), body["password"].s());
        } else {
            return crow::response(400, "{\"error\":\"Invalid account type\"}");
        }

        acc->saveAccountData();
        delete acc;

        string token = generateUserToken();
        user_tokens[token] = {accNum, time(0)};
        crow::json::wvalue res;
        res["success"] = true;
        res["token"] = token;
        res["accountNumber"] = accNum;
       crow::response response(201);
response.set_header("Content-Type", "application/json");
response.write(res.dump());
return response;
 
    });

  CROW_ROUTE(app, "/api/admin/login").methods("POST"_method)([](const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body) return crow::response(400, "Invalid JSON");

    string username = body["username"].s();
    string password = body["password"].s();

    if (username == "admin" && password == "admin123") {
        string token = generateAdminToken(username);
        crow::json::wvalue res;
        res["token"] = token;
        res["username"] = username;
        return crow::response(res);
    }

    return crow::response(401, "Invalid credentials");
});


CROW_ROUTE(app, "/api/account/activate").methods("PUT"_method)
([](const crow::request& req) {
    return handle_account_op(req, [](Account* acc) {
        acc->setAccountActive(true);
        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Account activated";
        return res;
    });
});
CROW_ROUTE(app, "/api/accounts/all_raw")
.methods("GET"_method)([&](const crow::request& req) {
    auto authHeader = req.get_header_value("Authorization");
    if (!verifyAdminToken(authHeader)) {
        return crow::response(401);
    }

    std::vector<Account*> accounts = Account::loadAllAccounts();
    crow::json::wvalue result;
    crow::json::wvalue::list accountList;

    for (const auto& acc : accounts) {
        if (acc) {
            crow::json::wvalue jsonAcc;
            jsonAcc["accountNumber"] = acc->getAccountNumber();
            jsonAcc["accountHolder"] = acc->getAccountHolder();
            jsonAcc["accountType"] = acc->getAccountType(); // must be "Savings" or "Current"
            jsonAcc["balance"] = acc->getBalance();
            jsonAcc["isActive"] = acc->isAccountActive();
            jsonAcc["createdAt"] = acc->getCreatedAt();  

            accountList.push_back(std::move(jsonAcc));
        }
    }

    result["accounts"] = std::move(accountList);

    for (auto acc : accounts) {
        delete acc;
    }

    return crow::response{result};
});


// NEW ROUTE: View all accounts with detailed information
CROW_ROUTE(app, "/api/accounts/view-all").methods("GET"_method)([&](const crow::request& req) {
    auto authHeader = req.get_header_value("Authorization");
    if (!verifyAdminToken(authHeader)) {
        return crow::response(401);  // Unauthorized
    }

    std::vector<Account*> accounts = Account::loadAllAccounts();
    std::vector<crow::json::wvalue> accountArray;

    for (Account* acc : accounts) {
        if (acc) {
            crow::json::wvalue accJson;
            accJson["accountNumber"] = acc->getAccountNumber();
            accJson["accountHolder"] = acc->getAccountHolder();
            accJson["accountType"] = acc->getAccountType(); // Returns integer
            accJson["balance"] = acc->getBalance();
            accJson["isActive"] = acc->isAccountActive();
            
            // Add account type name for better readability
            accJson["accountTypeName"] = acc->getAccountTypeName();
            
            accountArray.push_back(std::move(accJson));
            delete acc; // Delete each account after processing
        }
    }

    crow::json::wvalue result;
    result["accounts"] = std::move(accountArray);
    return crow::response(result);
});


CROW_ROUTE(app, "/api/account/login").methods("POST"_method)([](const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body) return crow::response(400);

    int accNum = body["accountNumber"].i();
    string password = body["password"].s();

    if (!Account::accountExists(accNum)) return crow::response(404);

    Account* acc = Account::loadAccountData(accNum, password);
    if (!acc || !acc->isAccountActive()) return crow::response(401);

    string token = generateTokenForAccount(accNum);
    crow::json::wvalue res;
    res["token"] = token;
    res["accountNumber"] = accNum;
    delete acc;
    return crow::response(res);
});

    CROW_ROUTE(app, "/api/accounts/all").methods("GET"_method)([](const crow::request& req) {
    string token = req.get_header_value("Authorization");

    if (!verifyAdminToken(token))
        return crow::response(401);

    vector<int> accs = Account::getAllAccountNumbers();
    vector<crow::json::wvalue> accountList;

    for (int num : accs) {
        Account* acc = Account::loadAccountData(num, "", true);
        if (acc && acc->isAccountActive()) {
            string type = acc->getAccountTypeName();
            if (type == "Savings Account" || type == "Current Account") {
                crow::json::wvalue obj;
                obj["accountNumber"] = acc->getAccountNumber();
                obj["accountHolder"] = acc->getAccountHolder();
                obj["balance"] = acc->getBalance();
                obj["type"] = type;
                accountList.push_back(std::move(obj));
            }
            delete acc;
        }
    }

    crow::json::wvalue res;
    res["accounts"] = std::move(accountList);
    return crow::response(res);
});

   CROW_ROUTE(app, "/api/loan/apply").methods("POST"_method)([&](const crow::request& req) {
    auto authHeader = req.get_header_value("Authorization");
    int accNum = -1;
    if (!verifyUserToken(authHeader, accNum)) {
        return crow::response(401, "Unauthorized");
    }

    auto body = crow::json::load(req.body);
    if (!body || !body.has("salary") || !body.has("loanAmount")) {
        return crow::response(400, "Invalid JSON body. Must include salary and loanAmount.");
    }

    double salary = body["salary"].d();
    double loanAmount = body["loanAmount"].d();

    if (salary < 15000 || loanAmount <= 0) {
        return crow::response(400, "Loan application failed: Invalid salary or loan amount.");
    }

    // Eligibility check
    double maxEligible = salary * 12 * 2.5;
    if (loanAmount > maxEligible) {
        crow::json::wvalue res;
        res["status"] = "rejected";
        res["reason"] = "Loan amount exceeds eligibility";
        res["maxEligible"] = maxEligible;
        return crow::response(403, res);
    }

    // Load and modify account
    Account* rawAcc = Account::loadAccountData(accNum, "", true);
    if (!rawAcc) return crow::response(404, "Account not found");

    // Cast to LoanAccount*
    LoanAccount* loanAcc = dynamic_cast<LoanAccount*>(rawAcc);
    if (!loanAcc) {
        delete rawAcc;
        return crow::response(400, "Account is not a LoanAccount");
    }

    // Set loan info
    loanAcc->setLoanAmount(loanAmount);
    loanAcc->setSalary(salary);
    loanAcc->saveAccountData();

    // Create response
    crow::json::wvalue res;
    res["status"] = "approved";
    res["loanAmount"] = loanAmount;
    res["salary"] = salary;
    res["interestRate"] = 8.5;
    res["monthlyEMI"] = calculateEMI(loanAmount, 8.5, 60);

    delete loanAcc;
    return crow::response(res);
});

CROW_ROUTE(app, "/api/account/deposit").methods("POST"_method)([](const crow::request& req) {
    string token = req.get_header_value("Authorization");
    int accNum = extractAccountNumberFromToken(token);
    if (accNum == -1) return crow::response(401);

    auto body = crow::json::load(req.body);
    if (!body) return crow::response(400);

    double amount = body["amount"].d();

    Account* acc = Account::loadAccountData(accNum, "", true);
    if (!acc) return crow::response(404);

    acc->deposit(amount);
    logTransaction(acc->getAccountNumber(), "DEPOSIT", amount);

    acc->saveAccountData();

    crow::json::wvalue res;
    res["newBalance"] = acc->getBalance();
    delete acc;
    return crow::response(res);
});


    CROW_ROUTE(app, "/api/account/withdraw").methods("PUT"_method)
    ([](const crow::request& req) {
        return handle_account_op(req, [&](Account* acc) {
            auto body = crow::json::load(req.body);
            double amt = body["amount"].d();
            if (amt <= 0) throw runtime_error("Invalid withdrawal amount");
            if (!acc->withdraw(amt)) throw runtime_error("Insufficient funds");
            logTransaction(acc->getAccountNumber(), "WITHDRAW", amt);
            crow::json::wvalue res;
            res["success"] = true;
            res["newBalance"] = acc->getBalance();
            return res;
        });
    });

    CROW_ROUTE(app, "/api/account/interest").methods("PUT"_method)
    ([](const crow::request& req) {
        return handle_account_op(req, [](Account* acc) {
            double before = acc->getBalance();
            acc->calculateInterest();
            double interest = acc->getBalance() - before;
            logTransaction(acc->getAccountNumber(), "INTEREST", interest);
            crow::json::wvalue res;
            res["success"] = true;
            res["newBalance"] = acc->getBalance();
            res["interest"] = interest;
            return res;
        });
    });

CROW_ROUTE(app, "/api/accounts/summary").methods("GET"_method)
([](const crow::request& req){
    crow::response res;

    // Token verification
    auto tokenHeader = req.get_header_value("Authorization");
    if  (tokenHeader.empty() || !verifyAdminToken(tokenHeader)) {
        res.code = 401;
        res.set_header("Content-Type", "application/json");
        res.write("{\"error\":\"Unauthorized\"}");
        return res;
    }

    // Summary counters
    int total = 0, savings = 0, current = 0, loan = 0, active = 0;
    vector<int> accNums = Account::getAllAccountNumbers();

    for (int accNum : accNums) {
        Account* acc = Account::loadAccountData(accNum, "", true);
        if (acc) {
            ++total;
            string type = acc->getAccountTypeName();
            if (type == "Savings Account") ++savings;
            else if (type == "Current Account") ++current;
            else if (type == "Loan Account") ++loan;
            if (acc->isAccountActive()) ++active;
            delete acc;
        }
    }

    crow::json::wvalue summary;
    summary["total"] = total;
    summary["savings"] = savings;
    summary["current"] = current;
    summary["loan"] = loan;
    summary["active"] = active;

    res.code = 200;
    res.set_header("Content-Type", "application/json");
    res.write(summary.dump());
    return res;
});

    CROW_ROUTE(app, "/api/account/deactivate").methods("PUT"_method)
    ([](const crow::request& req) {
        return handle_account_op(req, [](Account* acc) {
            acc->setAccountActive(false);
            crow::json::wvalue res;
            res["success"] = true;
            res["message"] = "Account deactivated";
            return res;
        });
    });

    CROW_ROUTE(app, "/api/account/details").methods("POST"_method)
([](const crow::request& req) {
    auto token = req.get_header_value("Authorization");
    int accNum = -1;

    if (!verifyUserToken(token, accNum)) {
        return crow::response(401, "{\"error\":\"Invalid or expired token\"}");
    }

    if (accNum == -1) {
        return crow::response(403, "{\"error\":\"Admin cannot view account details\"}");
    }

    Account* acc = Account::loadAccountData(accNum, "", true);
    if (!acc || !acc->isAccountActive()) {
        delete acc;
        return crow::response(404, "{\"error\":\"Account not found or inactive\"}");
    }

    crow::json::wvalue res;
    res["accountNumber"] = acc->getAccountNumber();
    res["accountHolder"] = acc->getAccountHolder();
    res["balance"] = acc->getBalance();
    res["accountType"] = acc->getAccountTypeName();
    delete acc;
    return crow::response(res);
});

    CROW_ROUTE(app, "/<path>")
([](const crow::request& req, crow::response& res, std::string path) {
    if (path.empty() || path == "/") {
        path = "start.html";
    }

    // Construct file path relative to current directory
    fs::path filepath = fs::current_path() / "frontend" / path;

    // Try opening the file
    std::ifstream file(filepath, std::ios::binary);

    // If not found and no .html extension, try adding .html
    if (!file && filepath.extension() != ".html") {
        filepath += ".html";
        file.open(filepath, std::ios::binary);
    }

    if (!file) {
        res.code = 404;
        res.set_header("Content-Type", "text/plain");
        res.write("404 - File Not Found: " + path);
        res.end();
        return;
    }

 std::ostringstream buffer;
buffer << file.rdbuf();
file.close();

res.set_header("Content-Type", getMimeType(filepath.string()));
res.write(buffer.str());
res.end();

});
int port = 18080;

if (const char* env_p = std::getenv("PORT")) {
    port = std::stoi(env_p);
}

app.port(port).run();
    return 0;
}