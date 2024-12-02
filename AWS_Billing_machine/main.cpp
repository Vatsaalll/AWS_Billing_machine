#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <cmath>
#include <iomanip>
#include <ctime>
#include <filesystem>

using namespace std;

// Structure to hold usage data
struct UsageRecord {
    string customerID;
    string instanceID;
    string instanceType;
    time_t usedFrom;
    time_t usedUntil;
};

// Structure to hold monthly usage details
struct MonthlyUsage {
    int resourceCount;
    double totalHours;
};

// Helper function to convert string to time_t
time_t parseTimestamp(const string& timestamp) {
    struct tm timeStruct = {};
    strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S", &timeStruct);
    return mktime(&timeStruct);
}

// Helper function to calculate hours used, split by months
map<string, double> calculateUsageByMonth(time_t start, time_t end) {
    map<string, double> hoursByMonth;
    while (start < end) {
        struct tm startTm = *localtime(&start);
        struct tm nextMonthTm = startTm;
        nextMonthTm.tm_mon++;
        nextMonthTm.tm_mday = 1;
        nextMonthTm.tm_hour = 0;
        nextMonthTm.tm_min = 0;
        nextMonthTm.tm_sec = 0;
        time_t nextBoundary = mktime(&nextMonthTm);

        time_t intervalEnd = min(nextBoundary, end);
        double hours = ceil(difftime(intervalEnd, start) / 3600.0);

        char monthKey[8];
        strftime(monthKey, sizeof(monthKey), "%b-%Y", &startTm);
        hoursByMonth[monthKey] += hours;

        start = intervalEnd;
    }
    return hoursByMonth;
}

// Function to read a CSV file and return a vector of vectors
vector<vector<string>> readCSV(const string& filepath) {
    vector<vector<string>> data;
    ifstream file(filepath);
    string line, cell;
    while (getline(file, line)) {
        vector<string> row;
        stringstream lineStream(line);
        while (getline(lineStream, cell, ',')) {
            row.push_back(cell);
        }
        data.push_back(row);
    }
    return data;
}

// Function to write CSV output
void writeCSV(const string& filepath, const vector<vector<string>>& data) {
    ofstream file(filepath);
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            file << row[i];
            if (i < row.size() - 1) file << ",";
        }
        file << "\n";
    }
}

int main() {
    // Read input CSVs
    auto usageData = readCSV("AWSResourceUsage.csv");
    auto resourceTypesData = readCSV("AWSResourceTypes.csv");
    auto customerData = readCSV("Customer.csv");

    // Map instance type to rate
    map<string, double> instanceRates;
    for (size_t i = 1; i < resourceTypesData.size(); ++i) { // Skip header
        instanceRates[resourceTypesData[i][0]] = stod(resourceTypesData[i][1].substr(1)); // Remove "$" and convert
    }

    // Map customer ID to customer name
    map<string, string> customerNames;
    for (size_t i = 1; i < customerData.size(); ++i) { // Skip header
        customerNames[customerData[i][0]] = customerData[i][1];
    }

    // Process usage records
    vector<UsageRecord> usageRecords;
    for (size_t i = 1; i < usageData.size(); ++i) { // Skip header
        UsageRecord record = {
            usageData[i][0],
            usageData[i][1],
            usageData[i][2],
            parseTimestamp(usageData[i][3]),
            parseTimestamp(usageData[i][4])
        };
        usageRecords.push_back(record);  
    }

    // Map for monthly billing
    map<string, map<string, map<string, MonthlyUsage>>> monthlyBills;

    // Aggregate usage data
    for (const auto& record : usageRecords) {
        auto usageByMonth = calculateUsageByMonth(record.usedFrom, record.usedUntil);
        for (const auto& [month, hours] : usageByMonth) {
            auto& usage = monthlyBills[record.customerID][month][record.instanceType];
            usage.resourceCount++;
            usage.totalHours += hours;
        }
    }

    // Generate output CSVs
    filesystem::create_directory("monthly_bills");
    for (const auto& [customerID, monthlyData] : monthlyBills) {
        string customerName = customerNames[customerID];
        for (const auto& [month, usageData] : monthlyData) {
            vector<vector<string>> output;
            output.push_back({"Instance Type", "Resource Count", "Total Hours", "Rate/Hour", "Cost"});
            double totalCost = 0;

            for (const auto& [instanceType, usage] : usageData) {
                double rate = instanceRates[instanceType];
                double cost = usage.totalHours * rate;
                totalCost += cost;

                output.push_back({
                    instanceType,
                    to_string(usage.resourceCount),
                    to_string(usage.totalHours),
                    "$" + to_string(rate),
                    "$" + to_string(cost)
                });
            }

            // Add summary row
            output.push_back({"", "", "", "Total Cost", "$" + to_string(totalCost)});

            // Write to CSV
            string filename = "monthly_bills/" + customerID + "_" + month + ".csv";
            writeCSV(filename, output);
        }
    }

    cout << "Monthly bills generated successfully in the 'monthly_bills' directory.\n";
    return 0;
}
