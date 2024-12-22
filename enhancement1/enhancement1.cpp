#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <unordered_map>
#include <ctime>
#include <map>

using namespace std;

struct Customer
{
    string customerId;
    string customerName;
};

struct ResourceType
{
    double chargePerHourOnDemand;
    double chargePerHourReserved;

    // Constructor
    ResourceType(double onDemand = 0.0, double reserved = 0.0)
        : chargePerHourOnDemand(onDemand), chargePerHourReserved(reserved) {}
};

struct RegionInfo
{
    string region;
    string freeTierInstanceType;
};

struct ResourceUsage
{
    string customerId;
    string instanceId;
    string resourceType;
    string usedFrom;
    string usedUntil;
    string region;
    string os;
    double hoursUsed;
};

struct ReservedInstance
{
    string customerId;
    string instanceId;
    string resourceType;
    string region;
    string os;
    double hourlyRate;
    int durationMonths;
};

struct BillItem
{
    string region;
    string resourceType;
    string os;
    int totalResources;
    double totalUsedTime;
    double totalBilledTime;
    double totalAmount;
    double discount;
    double actualAmount;
};

unordered_map<string, Customer> customers;
unordered_map<string, ResourceType> resourceTypes;
unordered_map<string, RegionInfo> regionInfos;
vector<ResourceUsage> onDemandUsages;
vector<ReservedInstance> reservedInstances;

string getMonthShortName(const string& monthNumber) {
    static const vector<string> monthNames = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                                              "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    int month = stoi(monthNumber);
    return monthNames[month - 1];
}

unordered_map<string, string> customerNameMap;

void loadCustomers(const string& fileName) {
    ifstream file(fileName);
    string line;
    getline(file, line); // Skip the header row
    while (getline(file, line)) {
        stringstream ss(line);
        string srNo, customerId, customerName;
        getline(ss, srNo, ',');
        getline(ss, customerId, ',');
        getline(ss, customerName, ',');
        customerNameMap[customerId] = customerName;
    }
    file.close();
}

void loadResourceTypes(const string &csvFilePath)
{
    ifstream file(csvFilePath);
    if (!file.is_open())
    {
        cerr << "Error: Unable to open AWSResourceTypes.csv file at " << csvFilePath << endl;
        return;
    }

    string line;
    getline(file, line); // Skip header

    while (getline(file, line))
    {
        stringstream ss(line);
        string field;

        vector<string> tokens;
        while (getline(ss, field, ','))
        {
            tokens.push_back(field);
        }

        if (tokens.size() < 5)
            continue; // Ensure valid row

        string instanceType = tokens[1];
        double chargePerHourOnDemand = stod(tokens[2]);
        double chargePerHourReserved = stod(tokens[3]);
        string region = tokens[4];

        // Create a unique key for region + instanceType
        string key = region + "_" + instanceType;

        // Populate resourceTypes map
        resourceTypes[instanceType] = {chargePerHourOnDemand, chargePerHourReserved};
    }

    file.close();
}

void loadRegionInfos(const string &filename)
{
    ifstream file(filename);
    string line;
    while (getline(file, line))
    {
        stringstream ss(line);
        string region, freeTierType;
        getline(ss, region, ',');
        getline(ss, freeTierType, ',');
        regionInfos[region] = {region, freeTierType};
    }
}

void loadOnDemandUsages(const string &filename)
{
    ifstream file(filename);
    string line;
    getline(file, line); // Skip header
    while (getline(file, line))
    {
        stringstream ss(line);
        string srNo, customerId, instanceId, resourceType, usedFrom, usedUntil, region, os;
        getline(ss, srNo, ',');
        getline(ss, customerId, ',');
        getline(ss, instanceId, ',');
        getline(ss, resourceType, ',');
        getline(ss, usedFrom, ',');
        getline(ss, usedUntil, ',');
        getline(ss, region, ',');
        getline(ss, os, ',');

        // Calculate hours used
        struct tm tmFrom = {}, tmUntil = {};
        sscanf(usedFrom.c_str(), "%d-%d-%dT%d:%d:%d",
               &tmFrom.tm_year, &tmFrom.tm_mon, &tmFrom.tm_mday,
               &tmFrom.tm_hour, &tmFrom.tm_min, &tmFrom.tm_sec);
        sscanf(usedUntil.c_str(), "%d-%d-%dT%d:%d:%d",
               &tmUntil.tm_year, &tmUntil.tm_mon, &tmUntil.tm_mday,
               &tmUntil.tm_hour, &tmUntil.tm_min, &tmUntil.tm_sec);

        tmFrom.tm_year -= 1900; // Adjust year
        tmFrom.tm_mon -= 1;     // Adjust month to 0-based
        tmUntil.tm_year -= 1900;
        tmUntil.tm_mon -= 1;

        time_t timeFrom = mktime(&tmFrom);
        time_t timeUntil = mktime(&tmUntil);
        double hoursUsed = difftime(timeUntil, timeFrom) / 3600.0;

        onDemandUsages.push_back({customerId, instanceId, resourceType, usedFrom, usedUntil, region, os, hoursUsed});
    }
}

void loadReservedInstances(const string &filename)
{
    ifstream file(filename);
    string line;
    getline(file, line); // Skip header
    while (getline(file, line))
    {
        stringstream ss(line);
        string customerId, instanceId, resourceType, region, os;
        double hourlyRate;
        int durationMonths;
        getline(ss, customerId, ',');
        getline(ss, instanceId, ',');
        getline(ss, resourceType, ',');
        getline(ss, region, ',');
        getline(ss, os, ',');
        ss >> hourlyRate;
        ss.ignore();
        ss >> durationMonths;

        reservedInstances.push_back({customerId, instanceId, resourceType, region, os, hourlyRate, durationMonths});
    }
}

void generateBills(const string& outputDir) {
    map<string, map<string, vector<BillItem>>> customerMonthlyBills;

    // Collect usage details per customer, month, and unique resource key
    for (const auto& usage : onDemandUsages) {
        string customerId = usage.customerId;
        string monthYear = usage.usedFrom.substr(0, 7); // Extract YYYY-MM
        string key = usage.region + "_" + usage.resourceType + "_" + usage.os;

        double rate = resourceTypes[usage.resourceType].chargePerHourOnDemand;

        // Check for reserved instance usage and override rate if applicable
        for (const auto& instance : reservedInstances) {
            if (instance.customerId == customerId && instance.resourceType == usage.resourceType &&
                instance.region == usage.region && instance.os == usage.os) {
                rate = instance.hourlyRate;
                break;
            }
        }

        // Prepare a BillItem for aggregation
        BillItem item = {usage.region, usage.resourceType, usage.os, 1, usage.hoursUsed, usage.hoursUsed,
                         usage.hoursUsed * rate, 0.0, 0.0};

        // Calculate discount if free tier applies
        if (regionInfos.count(usage.region) && regionInfos[usage.region].freeTierInstanceType == usage.resourceType) {
            item.discount = usage.hoursUsed * resourceTypes[usage.resourceType].chargePerHourOnDemand;
        }

        item.actualAmount = item.totalAmount - item.discount;
        customerMonthlyBills[customerId][monthYear].push_back(item);
    }

    // Generate bills
    for (const auto& customerBill : customerMonthlyBills) {
        const string& customerId = customerBill.first;

        for (const auto& monthBill : customerBill.second) {
            string monthYear = monthBill.first;
            string monthShortName = getMonthShortName(monthYear.substr(5, 2)); // Convert month to short form
            string year = monthYear.substr(0, 4);

            vector<BillItem>& billItems = const_cast<vector<BillItem>&>(monthBill.second);

            // Prepare output file
            stringstream filename;
            filename << outputDir << "/" << customerId << "_" << monthShortName << "-" << year << ".csv";

            ofstream file(filename.str());

            // Write customer and bill header
            file << "Customer: " << customerNameMap[customerId] << endl; // Add customer name from map
            file << "Bill for month of " << monthShortName << " " << year << endl;

            // Compute totals
            double totalAmount = 0.0, totalDiscount = 0.0, totalActualAmount = 0.0;

            // Write table header
            file << "Region,Resource Type,OS,Total Resources,Total Used Time (Hours),Total Billed Time (Hours),Total Amount,Discount,Actual Amount" << endl;

            // Write each bill item
            for (const auto& item : billItems) {
                file << item.region << "," << item.resourceType << "," << item.os << ","
                     << item.totalResources << "," << fixed << setprecision(2) << item.totalUsedTime << ","
                     << item.totalBilledTime << "," << fixed << setprecision(2) << item.totalAmount << "," << item.discount << ","
                     << item.actualAmount << endl;

                totalAmount += item.totalAmount;
                totalDiscount += item.discount;
                totalActualAmount += item.actualAmount;
            }

            // Write totals
            file << endl; // Add spacing before totals
            file << "Total Amount: $" << fixed << setprecision(2) << totalAmount << endl;
            file << "Total Discount: $" << fixed << setprecision(2) << totalDiscount << endl;
            file << "Actual Amount: $" << fixed << setprecision(2) << totalActualAmount << endl;
        }
    }
}

int main()
{
    string customersFile = "Customer.csv";
    string resourceTypesFile = "AWSResourceTypes.csv";
    string regionInfosFile = "Region.csv";
    string onDemandUsagesFile = "AWSOnDemandResourceUsage.csv";
    string reservedInstancesFile = "AWSReservedInstanceUsage.csv";

    loadCustomers(customersFile);
    loadResourceTypes(resourceTypesFile);
    loadRegionInfos(regionInfosFile);
    loadOnDemandUsages(onDemandUsagesFile);
    loadReservedInstances(reservedInstancesFile);

    string outputDir;
    cout << "Enter the output directory name: ";
    cin >> outputDir;

    generateBills(outputDir);

    return 0;
}
