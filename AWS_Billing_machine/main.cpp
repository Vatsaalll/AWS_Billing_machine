#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <iomanip>
#include <cmath>
#include <sys/stat.h>

using namespace std;

// Function to parse a CSV file
template <typename Func>
void parseCSV(const string& filename, Func handler) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Unable to open file " << filename << endl;
        return;
    }

    string line;
    // Skip the header line
    getline(file, line);

    while (getline(file, line)) {
        handler(line);
    }

    file.close();
}

// Function to split a string by delimiter
vector<string> split(const string& str, char delimiter) {
    vector<string> tokens;
    istringstream stream(str);
    string token;
    while (getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to convert hours to HH:mm:ss format
string formatTime(double hours) {
    int totalSeconds = static_cast<int>(round(hours * 3600));
    int hh = totalSeconds / 3600;
    int mm = (totalSeconds % 3600) / 60;
    int ss = totalSeconds % 60;
    ostringstream formattedTime;
    formattedTime << setw(2) << setfill('0') << hh << ":"
                  << setw(2) << setfill('0') << mm << ":"
                  << setw(2) << setfill('0') << ss;
    return formattedTime.str();
}

// Function to check if a directory exists
bool directoryExists(const string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

int main() {
    // Output directory for generated CSV files
    string outputDirectory;
    cout << "Enter the directory to save output files: ";
    cin >> outputDirectory;

    // Ensure the output directory exists
    if (!directoryExists(outputDirectory)) {
        cerr << "Error: Directory " << outputDirectory << " does not exist. Exiting program." << endl;
        return 1;
    }

    // Ensure the directory path ends with '/'
    if (outputDirectory.back() != '/' && outputDirectory.back() != '\\') {
        outputDirectory += '/';
    }

    // Maps to store data
    map<string, string> customerNames; // CustomerID -> CustomerName
    map<string, map<string, map<string, double>>> resourceUsage; // CustomerID -> Month -> ResourceType -> Hours
    map<string, double> resourceRates; // ResourceType -> Rate per Hour

    // Parse Customer.csv
    parseCSV("Customer.csv", [&](const string& line) {
        vector<string> tokens = split(line, ',');
        if (tokens.size() >= 2) {
            string customerID = tokens[0];
            string customerName = tokens[1];
            customerNames[customerID] = customerName;
        }
    });

    // Parse AWSResourceTypes.csv
    parseCSV("AWSResourceTypes.csv", [&](const string& line) {
        vector<string> tokens = split(line, ',');
        if (tokens.size() >= 2) {
            string resourceType = tokens[0];
            double rate = stod(tokens[1]);
            resourceRates[resourceType] = rate;
        }
    });

    // Parse AWSResourceUsage.csv
    parseCSV("AWSResourceUsage.csv", [&](const string& line) {
        vector<string> tokens = split(line, ',');
        if (tokens.size() >= 4) {
            string customerID = tokens[0];
            string resourceType = tokens[1];
            string month = tokens[2].substr(0, 7); // Extract YYYY-MM
            double hours = stod(tokens[3]);

            resourceUsage[customerID][month][resourceType] += hours;
        }
    });

    // Generate bills and write to CSV
    for (const auto& customer : resourceUsage) {
        const string& customerID = customer.first;
        const auto& monthlyUsage = customer.second;

        for (const auto& monthEntry : monthlyUsage) {
            const string& month = monthEntry.first;
            const auto& resources = monthEntry.second;
            double totalAmount = 0.0;

            // Calculate total amount
            for (const auto& resource : resources) {
                const string& resourceType = resource.first;
                double hours = resource.second;
                totalAmount += ceil(hours) * resourceRates[resourceType];
            }

            // Write to CSV file
            ostringstream filename;
            filename << outputDirectory << customerID << "_" << month << ".csv";
            ofstream outFile(filename.str());

            if (!outFile.is_open()) {
                cerr << "Error: Unable to write to file " << filename.str() << endl;
                continue;
            }

            // Write CSV content
            outFile << customerNames[customerID] << "\n";
            outFile << "Bill for month of " << month << "\n";
            outFile << "Total Amount: $" << fixed << setprecision(4) << totalAmount << "\n\n";
            outFile << "Resource Type,Total Resources,Total Used Time (HH:mm:ss),Total Billed Time (HH:mm:ss),Rate (per hour),Total Amount\n";

            for (const auto& resource : resources) {
                const string& resourceType = resource.first;
                double totalHours = resource.second;
                double billedHours = ceil(totalHours); // Assume full billing hours
                double rate = resourceRates[resourceType];
                double amount = billedHours * rate;

                outFile << resourceType << ",1," << formatTime(totalHours) << "," << formatTime(billedHours) << "," << fixed << setprecision(4) << rate << "," << amount << "\n";
            }

            outFile.close();
            cout << "Generated bill: " << filename.str() << endl;
        }
    }

    return 0;
}
