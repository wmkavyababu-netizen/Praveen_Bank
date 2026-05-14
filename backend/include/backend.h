#ifndef BACKEND_H
#define BACKEND_H

#include <string>
#include <vector>

// ---------------------- Base Account Class ----------------------
class Account {
protected:
    int accountNumber;
    std::string accountHolder;
    double balance;
    std::string password;
    bool isActive;
    double loanAmount = 0.0;
    double salary = 0.0;
    std::string branch;
    std::string ifsc;
    std::string createdDate;

public:
    Account(int accNum, std::string holder, double bal, std::string pass);
    virtual ~Account();

    std::string getCreatedAt() const {
    return createdDate;
}


    std::string getAccountHolder() const;
    int getAccountNumber() const { return accountNumber; }

    // Activation status
    bool isAccountActive() const;
    void setAccountActive(bool value);

    // Branch Info
    const std::string& getBranch() const { return branch; }
    const std::string& getIFSC() const { return ifsc; }
    const std::string& getCreatedDate() const { return createdDate; }
    void setBranch(const std::string& b) { branch = b; }
    void setIFSC(const std::string& i) { ifsc = i; }
    void setCreatedDate(const std::string& d) { createdDate = d; }

    // Balance
    double getBalance() const { return balance; }
    void setBalance(double b) { balance = b; }

    // Loan & Salary
    void setLoanAmount(double amt) { loanAmount = amt; }
    void setSalary(double sal) { salary = sal; }
    double getLoanAmount() const { return loanAmount; }
    double getSalary() const { return salary; }

    // Account operations
    virtual void displayAccountDetails();
    virtual void deposit(double amount);
    virtual bool withdraw(double amount);
    virtual void calculateInterest() = 0;

    // Type
    virtual int getAccountType() const = 0;
    virtual std::string getAccountTypeName() const = 0;

    // Save/load
    virtual void saveAccountData() const;
    static Account* loadAccountData(int accNum, const std::string& inputPass, bool skipPasswordCheck = false);
    static std::vector<Account*> loadAllAccounts();
    static bool accountExists(int accNum);
    static std::vector<int> getAllAccountNumbers();
    static void updateCentralAccountData();
};

// ---------------------- Derived Account Types ----------------------
class SavingsAccount : public Account {
public:
    SavingsAccount(int accNum, std::string holder, double bal, std::string pass);
    void calculateInterest() override;
    int getAccountType() const override;
    std::string getAccountTypeName() const override;
};

class CurrentAccount : public Account {
public:
    CurrentAccount(int accNum, std::string holder, double bal, std::string pass);
    void calculateInterest() override;
    int getAccountType() const override;
    std::string getAccountTypeName() const override;
};

class LoanAccount : public Account {
private:
    double loanInterestRate;
    int loanTermMonths;

public:
    LoanAccount(int accNum, std::string holder, double bal, std::string pass);
    void calculateInterest() override;
    int getAccountType() const override;
    std::string getAccountTypeName() const override;

    double getLoanInterestRate() const;
    int getLoanTermMonths() const;
    void repayLoan(double amount);
};

// ---------------------- Users ----------------------
class User {
protected:
    std::string username;
    std::string role;

public:
    User(std::string uname, std::string r);
    virtual ~User();
    virtual void showRole();
    virtual void adminMenu();
};

class Customer : public User {
public:
    Customer(std::string uname);
};

class Admin : public User {
public:
    Admin(std::string uname);
    void showRole() override;
    void adminMenu() override;
    void generateAccountReport();
    void viewAllAccounts();
};

#endif // BACKEND_H