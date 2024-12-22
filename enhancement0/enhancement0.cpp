#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <iomanip>
#include <ctime>
#include <math.h>

using namespace std;

// Function to calculate hours between two timestamps
double calculateHours(const string &startTime, const string &endTime)
{
    struct tm start{}, end{};

    istringstream ssStart(startTime);
    istringstream ssEnd(endTime);

    ssStart >> get_time(&start, "%Y-%m-%dT%H:%M:%S");
    ssEnd >> get_time(&end, "%Y-%m-%dT%H:%M:%S");

    if (ssStart.fail() || ssEnd.fail())
    {
        cerr << "Error: Time format invalid for: " << startTime << " or " << endTime << endl;
        return 0.0;
    }

    time_t startEpoch = mktime(&start);
    time_t endEpoch = mktime(&end);

    if (startEpoch == -1 || endEpoch == -1)
    {
        cerr << "Error: mktime failed for: " << startTime << " or " << endTime << endl;
        return 0.0;
    }

    return difftime(endEpoch, startEpoch) / 3600.0; // Difference in hours
}

// Function to convert numeric month to name
string getMonthName(int month)
{
    const string months[] = {"January", "February", "March", "April", "May", "June",
                             "July", "August", "September", "October", "November", "December"};
    if (month < 1 || month > 12)
        return "Invalid";
    return months[month - 1];
}

void generateMonthlyBills(const string &customerFile, const string &resourceTypeFile, const string &usageFile, const string &outputDirectory)
{
    ifstream customerStream(customerFile);
    ifstream resourceTypeStream(resourceTypeFile);
    ifstream usageStream(usageFile);

    if (!customerStream || !resourceTypeStream || !usageStream)
    {
        cerr << "Error: Unable to open input files." << endl;
        return;
    }

    // Load customer data
    map<string, string> customers;
    string line;
    getline(customerStream, line); // Skip header
    while (getline(customerStream, line))
    {
        stringstream ss(line);
        string srNo, customerID, customerName;
        getline(ss, srNo, ',');
        getline(ss, customerID, ',');
        getline(ss, customerName, ',');
        customers[customerID] = customerName; // Map customerID to customerName
    }

    // Load resource type data
    map<string, double> resourceRates;
    getline(resourceTypeStream, line); // Skip header
    while (getline(resourceTypeStream, line))
    {
        stringstream ss(line);
        string srNo, instanceType, chargePerHour;
        getline(ss, srNo, ',');
        getline(ss, instanceType, ',');
        getline(ss, chargePerHour, ',');
        resourceRates[instanceType] = stod(chargePerHour.substr(1)); // Remove '$' and convert to double
    }

    // Process usage data
    map<string, map<string, map<string, double>>> monthlyUsage; // customerID -> monthYear -> resourceType -> totalHours
    getline(usageStream, line);                                 // Skip header
    while (getline(usageStream, line))
    {
        stringstream ss(line);
        string srNo, customerID, instanceID, instanceType, usedFrom, usedUntil;
        getline(ss, srNo, ',');
        getline(ss, customerID, ',');
        getline(ss, instanceID, ',');
        getline(ss, instanceType, ',');
        getline(ss, usedFrom, ',');
        getline(ss, usedUntil, ',');

        double hoursUsed = calculateHours(usedFrom, usedUntil);
        string monthYear = usedFrom.substr(0, 7); // Extract YYYY-MM

        monthlyUsage[customerID][monthYear][instanceType] += hoursUsed;
    }

    // Generate monthly bills
    for (const auto &customer : monthlyUsage)
    {
        const string &customerID = customer.first;
        for (const auto &monthData : customer.second)
        {
            const string &monthYear = monthData.first;
            const auto &resourceUsage = monthData.second;

            // Extract month and year
            string year = monthYear.substr(0, 4);
            int month = stoi(monthYear.substr(5, 2));
            string monthName = getMonthName(month);

            // Prepare output file
            string outputFile = outputDirectory + "/" + customerID + "_" + monthName.substr(0, 3) + "-" + year + ".csv";
            ofstream outFile(outputFile);
            if (!outFile)
            {
                cerr << "Error: Unable to create file " << outputFile << endl;
                continue;
            }

            double totalAmount = 0.0;

            if (customers.find(customerID) != customers.end())
            {
                outFile << customers[customerID] << "\n";
            }

            // Write bill header
            outFile << "Bill for month of " << monthName << " " << year << "\n";

            // Write resource usage
            outFile << "Resource Type,Total Resources,Total Used Time (HH:mm:ss),Total Billed Time (HH:mm:ss),Rate (per hour),Total Amount\n";

            for (const auto &usage : resourceUsage)
            {
                const string &resourceType = usage.first;
                double totalHours = usage.second;
                double ratePerHour = resourceRates[resourceType];
                double amount = totalHours * ratePerHour;
                totalAmount += amount;

                int totalResources = 1; // Assuming 1 resource type per instance
                int billedHours = ceil(totalHours);

                outFile << resourceType << ","
                        << totalResources << ","
                        << fixed << setprecision(2) << totalHours << ","
                        << billedHours << ":00:00,"
                        << "$" << ratePerHour << ","
                        << "$" << amount << "\n";
            }

            outFile << "Total Amount: $" << fixed << setprecision(2) << totalAmount << "\n";
            outFile.close();
            cout << "Generated bill: " << outputFile << endl;
        }
    }
}

int main()
{
    string customerFile = "Customer.csv";
    string resourceTypeFile = "AWSResourceTypes.csv";
    string usageFile = "AWSResourceUsage.csv";
    string outputDirectory;

    cout << "Enter the directory to save output files: ";
    cin >> outputDirectory;

    generateMonthlyBills(customerFile, resourceTypeFile, usageFile, outputDirectory);

    return 0;
}
