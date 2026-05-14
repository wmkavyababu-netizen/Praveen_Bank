#include "../include/backend.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <vector>
#include "crow.h"
#include <filesystem>
namespace fs = std::filesystem;

using namespace std;

// ------ Account Part ------
bool Account::accountExists(int accNum) {
    fs::path base = fs::current_path() / "accounts" / (to_string(accNum) + ".dat");
    ifstream file(base.string());
    return file.good();
}

// Account constructor
Account::Account(int accNum, string holder, double bal, string pass) : 
    accountNumber(accNum), accountHolder(holder), balance(bal), password(pass), isActive(true) {}

Account::~Account() {}

string Account::getAccountHolder() const {
    return accountHolder;
}

bool Account::isAccountActive() const {
    return isActive;
}

void Account::setAccountActive(bool value) {
    isActive = value;
}

// Member function implementations
void Account::displayAccountDetails() {
    cout << "\n--- Account Details ---\n";
    cout << "Account Number : " << accountNumber << endl;
    cout << "Account Holder : " << accountHolder << endl;
    cout << "Balance        : " << balance << endl;
}

void Account::deposit(double amount) {
    if (amount > 0) {
        balance += amount;
        cout << "Deposited: " << amount << "    New Balance: " << balance << endl;
    }
}

bool Account::withdraw(double amount) {
    if (balance >= amount && amount > 0) {
        balance -= amount;
        cout << "Withdrawn: " << amount << "    New Balance: " << balance << endl;
        return true;
    } else {
        cout << "Insufficient Balance!" << endl;
        return false;
    }
}

void Account::saveAccountData() const {
    fs::path base = fs::current_path() / "accounts" / (to_string(accountNumber) + ".dat");
    ofstream file(base.string(), ios::binary);

    file << accountNumber << '\n';
    file << accountHolder << '\n';
    file << balance << '\n';
    file << password << '\n';
    file << getAccountType() << '\n';
    file << isActive << '\n';
    file << loanAmount << '\n';
    file << salary << '\n';
    file << branch << '\n';
    file << ifsc << '\n';
    file << createdDate << '\n';
    
    updateCentralAccountData();
}

void Account::updateCentralAccountData() {
    vector<int> accNumbers = getAllAccountNumbers();
    ofstream dataFile("account-data.txt", ios::trunc);

    for (int accNum : accNumbers) {
        Account* acc = loadAccountData(accNum, "", true);
        if (acc) {
            dataFile << acc->accountNumber << '\n';
            dataFile << acc->accountHolder << '\n';
            dataFile << acc->balance << '\n';
            dataFile << acc->getAccountTypeName() << "\n\n";
            delete acc;
        }
    }
    dataFile.close();
}

Account* Account::loadAccountData(int accNum, const string& inputPass, bool skipPasswordCheck) {
    fs::path base = fs::current_path() / "accounts" / (to_string(accNum) + ".dat");
    ifstream file(base.string(), ios::binary);

    if (file.is_open()) {
        int num;
        string holder;
        double bal;
        string pass;
        int accType;
        bool active;
        double loanAmt, sal;
        string br, ifscCode, date;

        file >> num;
        if (file.fail()) return nullptr;
        file.ignore();
        getline(file, holder);
        if (file.fail()) return nullptr;
        file >> bal;
        if (file.fail()) return nullptr;
        file.ignore();
        getline(file, pass);
        if (file.fail()) return nullptr;
        file >> accType;
        if (file.fail()) return nullptr;
     //   file >> active;
        int activeInt = 0;
        file >> activeInt;
        active = (activeInt != 0);

        if (file.fail()) return nullptr;
        file >> loanAmt >> sal;
        if (file.fail()) return nullptr;
        file.ignore();
        getline(file, br);
        if (file.fail()) return nullptr;
        getline(file, ifscCode);
        if (file.fail()) return nullptr;
        getline(file, date);
        if (file.fail()) return nullptr;

        if (!skipPasswordCheck && pass != inputPass) return nullptr;

        Account* acc = nullptr;
        switch (accType) {
            case 1: acc = new SavingsAccount(num, holder, bal, pass); break;
            case 2: acc = new CurrentAccount(num, holder, bal, pass); break;
            case 3: acc = new LoanAccount(num, holder, bal, pass); break;
            default: return nullptr;
        }

        if (acc) {
            acc->setAccountActive(active);
            acc->setLoanAmount(loanAmt);
            acc->setSalary(sal);
            acc->setBranch(br);
            acc->setIFSC(ifscCode);
            acc->setCreatedDate(date);
        }
        return acc;
    }
    return nullptr;
}

vector<Account*> Account::loadAllAccounts() {
    vector<Account*> accounts;
    vector<int> accNumbers = getAllAccountNumbers();
    for (int num : accNumbers) {
        Account* acc = loadAccountData(num, "", true);
        if (acc) accounts.push_back(acc);
    }
    return accounts;
}

vector<int> Account::getAllAccountNumbers() {
    vector<int> numbers;
    fs::path basePath = fs::current_path() / "accounts";

    if (!fs::exists(basePath)) {
        cerr << "❌ accounts folder not found at: " << basePath << endl;
        return numbers;
    }

    for (const auto& entry : fs::directory_iterator(basePath)) {
        string filename = entry.path().filename().string();
        if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".dat") {
            try {
                int accNum = stoi(filename.substr(0, filename.size() - 4));
                numbers.push_back(accNum);
            } catch (...) {
                continue;
            }
        }
    }
    return numbers;
}

// ------ SavingsAccount Methods ------
SavingsAccount::SavingsAccount(int accNum, string holder, double bal, string pass) : 
    Account(accNum, holder, bal, pass) {}

void SavingsAccount::calculateInterest() {
    double interest = balance * 0.04;
    balance += interest;
    cout << "Savings Interest (4%) Added: " << interest << "  New Balance: " << balance << endl;
}

int SavingsAccount::getAccountType() const {
    return 1;
}

string SavingsAccount::getAccountTypeName() const {
    return "Savings Account";
}

// ------ CurrentAccount Methods ------
CurrentAccount::CurrentAccount(int accNum, string holder, double bal, string pass) : 
    Account(accNum, holder, bal, pass) {}

void CurrentAccount::calculateInterest() {
    cout << "No interest for Current Account." << endl;
}

int CurrentAccount::getAccountType() const {
    return 2;
}

string CurrentAccount::getAccountTypeName() const {
    return "Current Account";
}

// ------ LoanAccount Methods ------
LoanAccount::LoanAccount(int accNum, string holder, double bal, string pass) : 
    Account(accNum, holder, bal, pass), loanInterestRate(0.10), loanTermMonths(12) {}

void LoanAccount::calculateInterest() {
    double interest = balance * 0.10;
    balance += interest;
    cout << "Loan Interest (10%) Added: " << interest << " | New Balance: " << balance << endl;
}

int LoanAccount::getAccountType() const {
    return 3;
}

string LoanAccount::getAccountTypeName() const {
    return "Loan Account";
}

double LoanAccount::getLoanInterestRate() const {
    return loanInterestRate;
}

int LoanAccount::getLoanTermMonths() const {
    return loanTermMonths;
}

void LoanAccount::repayLoan(double amount) {
    if (amount <= 0) {
        cout << "❌ Invalid repayment amount.\n";
        return;
    }

    if (amount >= balance) {
        cout << "✅ Full loan repaid! Extra amount refunded: " << (amount - balance) << endl;
        balance = 0;
    } else {
        balance -= amount;
        cout << "💰 Partial repayment done. Remaining loan: " << balance << endl;
    }
    saveAccountData();
}

// ------ User Methods ------
User::User(string uname, string r) : username(uname), role(r) {}

void User::showRole() {
    cout << "\nLogged in as " << role << ": " << username << endl;
}

void User::adminMenu() {}

User::~User() {}

Customer::Customer(string uname) : User(uname, "Customer") {}

Admin::Admin(string uname) : User(uname, "Admin") {}

void Admin::showRole() {
    cout << "Welcome Admin: " << username << endl;
}

void Admin::adminMenu() {
    int choice;
    do {            
        cout << endl << "\nAdmin Menu:\n";
        cout << "1. Generate Account Report\n";
        cout << "2. View All Accounts\n";
        cout << "3. Exit Admin Menu\n";
        cout << "Enter your choice: ";
        cin >> choice;

        switch (choice) {
            case 1: generateAccountReport(); break;
            case 2: viewAllAccounts(); break;
            case 3: cout << "Exiting admin menu.\n"; break;
            default: cout << "Invalid choice. Try again.\n";
        }
    } while (choice != 3);
}

void Admin::generateAccountReport() {
    int savingsCount = 0, currentCount = 0, loanCount = 0;
    vector<int> accs = Account::getAllAccountNumbers();

    for (int num : accs) {
        Account* acc = Account::loadAccountData(num, "", true);
        if (acc) {
            switch (acc->getAccountType()) {
                case 1: savingsCount++; break;
                case 2: currentCount++; break;
                case 3: loanCount++; break;
            }
            delete acc;
        }
    }

    cout << " \n--- Account Report ---\n";
    cout << "Total Accounts: " << accs.size() << endl;
    cout << "Savings Accounts: " << savingsCount << endl;
    cout << "Current Accounts: " << currentCount << endl;
    cout << "Loan Accounts: " << loanCount << endl;
}

void Admin::viewAllAccounts() {
    vector<int> accs = Account::getAllAccountNumbers();
    if (accs.empty()) {
        cout << "No accounts found!" << endl;
        return;
    }

    for (int num : accs) {
        Account* acc = Account::loadAccountData(num, "", true);
        if (acc) {
            cout << "\nAccount Number: " << acc->getAccountNumber() << endl;
            cout << "Account Holder: " << acc->getAccountHolder() << endl;
            cout << "Balance: " << acc->getBalance() << endl;
            cout << "Account Type: " << acc->getAccountTypeName() << endl;
            delete acc;
        }
    }
}