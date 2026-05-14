#include "../include/backend.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
using namespace std;
// ------ Integrating all done by KAVIYA  ------ 

int main() 
{
    int choice;
    cout << endl << "\t \t \t \t \t WELCOME TO PRAVEEN BANK" << endl << endl;
    cout << endl << endl << "Choose any one (1/2) : " << endl << "1. Customer" << endl << "2. Admin" << endl;
    cout << "Your Choice : ";
    cin >> choice;

    Account *account = nullptr;
    User *user = nullptr;

    if (choice == 1) 
    {
        int choice1;
        cout << "Choose any one" << endl << "1. First Time User" << endl << "2. Already Existing User" << endl;
        cout << endl<<"Your Choice : ";
        cin >> choice1;

        if (choice1 == 1) 
        {
            string holder, pass;
            double bal;
            int accType, accNum;

            cout << endl << "Enter account number: ";
            cin >> accNum;

            if (Account::accountExists(accNum))
            {
                cout << "Account number already exists! Please choose a different number." << endl;
                return 1;
            }

            cout << "Enter account holder name: ";
            cin.ignore();
            getline(cin, holder);
            cout << "Enter Amount to Deposit: ";
            cin >> bal;
            cout << "Create a password: ";
            cin.ignore();
            getline(cin, pass);

            cout << endl << "Select account type:\n";
            cout << "1. Savings Account\n";
            cout << "2. Current Account\n";
            cout << "3. Loan Account\n";
            cout << "Enter your choice: ";
            cin >> accType;

            if (accType == 1) 
                account = new SavingsAccount(accNum, holder, bal, pass);
            else if (accType == 2) 
                account = new CurrentAccount(accNum, holder, bal, pass);
            else if (accType == 3) 
                account = new LoanAccount(accNum, holder, bal, pass);
            else 
            {
                cout << "Invalid account type!" << endl;
                return 1;
            }

            account->saveAccountData();
            cout << endl <<"Account created successfully!" << endl;
            user = new Customer(holder);
        } 
        else if (choice1 == 2) 
        {
            int accNum;
            string inputPass;
            cout << "Enter account number: ";
            cin >> accNum;
            cin.ignore();
            cout << "Enter password: ";
            getline(cin, inputPass);

            account = Account::loadAccountData(accNum, inputPass);
            if (account)  
            {
                user = new Customer(account->getAccountHolder());
            }
        }
    } 
    else if (choice == 2) 
    {
        user = new Admin("Bank Admin");
        user->showRole();
        static_cast<Admin*>(user)->adminMenu();
        delete user;
        return 0;
    }
    else 
    {
        cout << "Invalid choice!" << endl;
        return 1;
    }

    if (account && user) 
    {
        user->showRole();
        int menuChoice;
        do 
        {
            cout << "\nMenu:\n";
            cout << "1. View Account Details\n";
            cout << "2. Deposit\n";
            cout << "3. Withdraw\n";
            cout << "4. Calculate Interest\n";
            cout << "5. Exit\n";
            cout << "Enter your choice: ";
            cin >> menuChoice;

            switch (menuChoice) 
            {
                case 1:
                    account->displayAccountDetails();
                    break;
                case 2: 
                {
                    double amount;
                    cout << "Enter deposit amount: ";
                    cin >> amount;
                    account->deposit(amount);
                    account->saveAccountData();
                    break;
                }
                case 3: 
                {
                    double amount;
                    cout << "Enter withdrawal amount: ";
                    cin >> amount;
                    account->withdraw(amount);
                    account->saveAccountData();
                    break;
                }
                case 4:
                    account->calculateInterest();
                    break;
                case 5:
                    cout << "Exiting program.\n";
                    break;
                default:
                    cout << "Invalid choice. Try again.\n";
            }
        } while (menuChoice != 5);
    }

    ofstream file("account_data.txt", ios::app);
    file.close();
    
    if (account) delete account;
    if (user) delete user;
    return 0;
}