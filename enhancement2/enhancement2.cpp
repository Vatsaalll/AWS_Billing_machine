#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <map>
#include <cmath>

struct ElasticIPRate
{
    std::string region;
    double ratePerHour;
};

struct ElasticIPAllocation
{
    std::string customer;
    std::string region;
    std::string elasticIP;
    std::tm usedFrom;
    std::tm usedUntil;
    bool isOwnIP;
};

struct ElasticIPAssociation
{
    std::string ipAddress;
    std::string ec2Instance;
    std::tm associatedFrom;
    std::tm associatedUntil;
};

bool isEarlier(const std::tm &a, const std::tm &b)
{
    return std::mktime(const_cast<std::tm *>(&a)) < std::mktime(const_cast<std::tm *>(&b));
}

std::tm getLater(const std::tm &a, const std::tm &b)
{
    return isEarlier(a, b) ? b : a;
}

std::tm getEarlier(const std::tm &a, const std::tm &b)
{
    return isEarlier(a, b) ? a : b;
}

std::tm parseTime(const std::string &timeStr)
{
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return tm;
}

double calculateTimeDifference(const std::tm &from, const std::tm &until)
{
    std::time_t fromTime = std::mktime(const_cast<std::tm *>(&from));
    std::time_t untilTime = std::mktime(const_cast<std::tm *>(&until));
    return std::difftime(untilTime, fromTime) / 3600.0; // Return hours
}

std::string getMonthName(int month)
{
    const std::vector<std::string> months = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    return months[month - 1];
}

void loadElasticIPRates(const std::string &filename, std::unordered_map<std::string, ElasticIPRate> &rates)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string region, ratePerHourStr;

        std::getline(ss, region, ',');
        std::getline(ss, ratePerHourStr, ',');

        rates[region] = {region, std::stod(ratePerHourStr.substr(1))}; // Skip '$'
    }
}

void loadElasticIPAllocations(const std::string &filename, std::vector<ElasticIPAllocation> &allocations)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        ElasticIPAllocation allocation;
        std::string usedFromStr, usedUntilStr, isOwnIPStr;

        std::getline(ss, allocation.customer, ',');
        std::getline(ss, allocation.region, ',');
        std::getline(ss, allocation.elasticIP, ',');
        std::getline(ss, usedFromStr, ',');
        std::getline(ss, usedUntilStr, ',');
        std::getline(ss, isOwnIPStr, ',');

        allocation.usedFrom = parseTime(usedFromStr);
        allocation.usedUntil = parseTime(usedUntilStr);
        allocation.isOwnIP = (isOwnIPStr == "Yes");

        allocations.push_back(allocation);
    }
    
}

void loadElasticIPAssociations(const std::string &filename, std::vector<ElasticIPAssociation> &associations)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        ElasticIPAssociation association;
        std::string associatedFromStr, associatedUntilStr;

        std::getline(ss, association.ipAddress, ',');
        std::getline(ss, association.ec2Instance, ',');
        std::getline(ss, associatedFromStr, ',');
        std::getline(ss, associatedUntilStr, ',');

        association.associatedFrom = parseTime(associatedFromStr);
        association.associatedUntil = parseTime(associatedUntilStr);

        associations.push_back(association);
    }
}

double calculateBilledTime(const ElasticIPAllocation &allocation, const std::vector<ElasticIPAssociation> &associations)
{
    double totalBilledTime = calculateTimeDifference(allocation.usedFrom, allocation.usedUntil);

    for (const auto &association : associations)
    {
        if (association.ipAddress == allocation.elasticIP)
        {
            // Check if there is an overlap
            std::tm overlapStart = getLater(allocation.usedFrom, association.associatedFrom);
            std::tm overlapEnd = getEarlier(allocation.usedUntil, association.associatedUntil);

            if (std::difftime(std::mktime(&overlapEnd), std::mktime(&overlapStart)) > 0)
            {
                double overlapTime = calculateTimeDifference(overlapStart, overlapEnd);
                totalBilledTime -= overlapTime; // Deduct overlapping time
            }
        }
    }

    return std::max(0.0, totalBilledTime); // Ensure billed time is not negative
}

void generateMonthlyBills(const std::vector<ElasticIPAllocation> &allocations,
                          const std::vector<ElasticIPAssociation> &associations,
                          const std::unordered_map<std::string, ElasticIPRate> &rates)
{
    std::string outputDirectory;
    std::cout << "Enter the directory where the output CSV files should be saved: ";
    std::getline(std::cin, outputDirectory);

    std::map<std::string, std::map<std::string, std::vector<ElasticIPAllocation>>> groupedAllocations;

    // Group allocations by customer and month-year
    for (const auto &allocation : allocations)
    {
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m", &allocation.usedFrom);
        groupedAllocations[allocation.customer][buffer].push_back(allocation);
    }
    

    for (const auto &customerEntry : groupedAllocations)
    {
        const std::string &customer = customerEntry.first;
        for (const auto &monthEntry : customerEntry.second)
        {
            const std::string &monthYear = monthEntry.first;
            const auto &monthlyAllocations = monthEntry.second;

            // Open output file
            std::tm monthTm = {};
            std::istringstream ss(monthYear + "-01");
            ss >> std::get_time(&monthTm, "%Y-%m-%d");
            std::string monthName = getMonthName(monthTm.tm_mon + 1);
            std::string filename = outputDirectory + "/" + customer + "_" + monthName + "-" + std::to_string(1900 + monthTm.tm_year) + ".csv";

            std::ofstream outFile(filename);
            if (!outFile.is_open())
            {
                std::cerr << "Failed to create file: " << filename << std::endl;
                continue;
            }

            outFile << "Customer: " << customer << "\n";
            outFile << "Bill for month of " << monthName << " " << (1900 + monthTm.tm_year) << "\n";
            outFile << "Region,IP Address,Total Allocation Time,Total Billed Time,Amount\n";

            double totalAmount = 0.0;

            for (const auto &allocation : monthlyAllocations)
            {

                if (allocation.isOwnIP)
                {
                    double allocationTime = calculateTimeDifference(allocation.usedFrom, allocation.usedUntil);
                    outFile << allocation.region << "," << allocation.elasticIP << "," << allocationTime << " hours,0 hours,$0.00\n";
                    continue;
                }
                double allocationTime = calculateTimeDifference(allocation.usedFrom, allocation.usedUntil);
                double billedTime = calculateBilledTime(allocation, associations);
                double amount = billedTime * rates.at(allocation.region).ratePerHour;

                outFile << allocation.region << "," << allocation.elasticIP << "," << allocationTime << " hours," << billedTime << " hours," << "$" << amount << "\n";
                totalAmount += amount;
            }

            

            outFile << "Total Amount: $" << std::fixed << std::setprecision(2) << totalAmount << "\n";
            outFile.close();
        }
    }
}

int main()
{
    std::unordered_map<std::string, ElasticIPRate> rates;
    std::vector<ElasticIPAllocation> allocations;
    std::vector<ElasticIPAssociation> associations;

    loadElasticIPRates("ElasticIPRates.csv", rates);
    loadElasticIPAllocations("ElasticIPAllocation.csv", allocations);
    loadElasticIPAssociations("ElasticIPAssociation.csv", associations);

    generateMonthlyBills(allocations, associations, rates);

    return 0;
}
