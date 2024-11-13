#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <iomanip>
#include <sstream>
#include <fstream>

using namespace std;

string wholekey;

// Define Table struct
struct Table {

    static int Total;
    vector<string> attributes, DataType, keys, FDs;
    unordered_map<string, vector<string>> Content;
    string name;


    // Constructor
    Table(const vector<string>& tableAttributes, const vector<string>& fd, const vector<string>& tableKeys, unordered_map<string, vector<string>> tableData, const vector<string> type)
        : name("Table" + to_string(++Total)), attributes(tableAttributes), FDs(fd), keys(tableKeys), Content(tableData), DataType(type) {}

    // Function to remove columns
    static void drop_attributes(Table& table, const vector<string>& attributesToRemove) {
        for (const string& attribute : attributesToRemove) {
            // Locate the attribute in the attributes vector
            auto it = find(table.attributes.begin(), table.attributes.end(), attribute);
            if (it != table.attributes.end()) {
                // Calculate index of the attribute
                size_t index = distance(table.attributes.begin(), it);

                // Debugging output to track removal process
                cout << "Removing attribute: " << attribute << " at index " << index << endl;

                // Remove the attribute and its corresponding type
                table.attributes.erase(table.attributes.begin() + index);
                if (index < table.DataType.size()) {
                    table.DataType.erase(table.DataType.begin() + index);
                } else {
                    cout << "Error: Type index out of range for attribute " << attribute << endl;
                }

                // Remove the attribute from data
                table.Content.erase(attribute);

                // Remove attribute from fundamental dependencies and keys
                table.FDs.erase(remove(table.FDs.begin(), table.FDs.end(), attribute), table.FDs.end());
                table.keys.erase(remove(table.keys.begin(), table.keys.end(), attribute), table.keys.end());
            } else {
                cout << "Attribute not found: " << attribute << endl;
            }
        }
    }
};

// Function to determine data types
string determineDataType(const string& input) {
    if (input.empty()) {
        return "EMPTY";
    }

    bool isInteger = true;
    for (char c : input) {
        if (!isdigit(c)) {
            isInteger = false;
            break;
        }
    }

    if (isInteger) {
        return "INT";
    } else {
        istringstream dateSS(input);
        int month, day, year;
        char delimiter;
        dateSS >> month >> delimiter >> day >> delimiter >> year;
        if (delimiter == '/') {
            return "DATE";
        } else {
            return "VARCHAR(255)";
        }
    }
}

string trim(const string& str) { 
    size_t first = str.find_first_not_of(" \n\r");
    size_t last = str.find_last_not_of(" \n\r");
    return (first == string::npos || last == string::npos) ? "" : str.substr(first, last - first + 1);
}

bool isFD(const vector<string>& lhs, const string& rhs, const unordered_map<string, vector<string>>& data) {
    // Use a map to track occurrences of lhs values and their corresponding rhs values
    unordered_map<string, string> lhsToRhsMap;

    for (size_t i = 0; i < data.at(lhs[0]).size(); ++i) {
        // Construct the lhs key from all lhs attributes
        string lhsKey;
        for (const auto& attr : lhs) {
            lhsKey += data.at(attr)[i] + "|"; // Use '|' as a delimiter for uniqueness
        }

        // Get the current rhs value
        const string& currentRhsValue = data.at(rhs)[i];

        // Check if we've seen this lhsKey before with a different rhs value
        if (lhsToRhsMap.find(lhsKey) != lhsToRhsMap.end()) {
            if (lhsToRhsMap[lhsKey] != currentRhsValue) {
                return false; // Dependency does not hold as multiple rhs values are found for the same lhs key
            }
        } else {
            // Map the lhsKey to the current rhs value
            lhsToRhsMap[lhsKey] = currentRhsValue;
        }
    }

    return true; // Dependency holds for all entries
}

// Checks if a set of attributes represents a join dependency
bool hasJoinDependency(const vector<string>& attrs, const vector<string>& candidateKeys, const unordered_map<string, vector<string>>& data) {
    unordered_set<string> attributeSet(attrs.begin(), attrs.end());
    for (const auto& key : candidateKeys) {
        if (attributeSet.find(key) != attributeSet.end()) {
            return false;  // If candidate key is part of the join, its not a true join dependency
        }
    }
    return true;
}

vector<string> parseFD2(const string& input) {
    vector<string> parsed;
    stringstream commaSplit(input);
    while (commaSplit.good()) {
        string data;
        getline(commaSplit, data, ',');
        parsed.push_back(data);
    }
    return parsed;
}

// Input Function
Table loadTableData(const string& filename, const string& fileFD) {
    // Parse FD
    string inputLine;
    ifstream parseFD(fileFD);
    vector<string> FDinput;
    if (parseFD.is_open()) {
        while (getline(parseFD, inputLine))
            FDinput.push_back(trim(inputLine));
        parseFD.close();
    }

    // Parse keys
    string keys;
    cout << "\nEnter keys separated by , e.g. (key1,key2,key3): " << endl;
    cin >> keys;
    wholekey = "{" + keys + "}";
    stringstream ks(keys);
    string keyHere;
    vector<string> parsedKeys;
    while (getline(ks, keyHere, ',')) {
        parsedKeys.push_back(keyHere);
    }

    ifstream file(filename);
    string line;
    getline(file, line);
    stringstream ss(line);
    vector<string> attributes;
    string attribute;

    // Parse attributes
    while (getline(ss, attribute, ',')) {
        attributes.push_back(attribute);
    }

    // Parse data types
    ifstream typeFile(filename);
    string csvLine, field;
    getline(typeFile, csvLine); // Skip header
    getline(typeFile, csvLine); // Read first row of data

    istringstream ss2(csvLine);
    vector<string> dataTypes;

    while (getline(ss2, field, ',')) {
        string dataType = determineDataType(field);
        dataTypes.push_back(dataType);
    }
    typeFile.close();

    // Create the table with attributes
    Table table(attributes, FDinput, parsedKeys, {}, dataTypes);

    // Parse actual data
    while (getline(file, line)) {
        stringstream ss(line);
        string value;
        vector<string> dataRow;
        bool inQuotes = false;
        string tempValue = "";

        while (getline(ss, value, ',')) {
            if (!inQuotes) {
                if (value.front() == '\"' && value.back() == '\"') {
                    dataRow.push_back(value.substr(1, value.size() - 2));
                } else if (value.front() == '\"') {
                    inQuotes = true;
                    tempValue = value;
                } else {
                    dataRow.push_back(value);
                }
            } else {
                tempValue += "," + value;
                if (value.back() == '\"') {
                    inQuotes = false;
                    dataRow.push_back(tempValue.substr(1, tempValue.size() - 2));
                }
            }
        }

        if (dataRow.size() != attributes.size()) {
            cerr << "Error: Number of columns in a row does not match the number of attributes." << endl;
            continue;
        }

        // Store data into the table
        for (size_t i = 0; i < attributes.size(); ++i) {
            table.Content[attributes[i]].push_back(dataRow[i]);
        }
    }
    file.close();

    return table;
}


// Output query results
void generateOutput(const vector<Table>& TablestobeOutput) {
    ofstream outputFile("output.txt");
    
    if (outputFile.is_open()) {
        for (const auto& query : TablestobeOutput) {
            outputFile << "CREATE TABLE " << query.name << " (\n";

            // Ensure attributes and types vectors have the same size
            if (query.attributes.size() != query.DataType.size()) {
                cerr << "Error: Mismatch between attributes and types in table " << query.name << endl;
                continue; // Skip this table and move on to the next one
            }

            // Iterate over the attributes and types
            for (size_t x = 0; x < query.attributes.size(); x++) {
                outputFile << "\t" << query.attributes[x] << " " << query.DataType[x];
                if (x < query.attributes.size() - 1) {
                    outputFile << ",\n";
                } else {
                    outputFile << "\n";
                }
            }

            outputFile << ");\n\n";
        }
        
        outputFile.close();
    } else {
        cerr << "Error: Unable to open output file." << endl;
    }
}

// Helper function to parse a functional dependency
pair<string, string> parseFD3(const string& fd) {
    size_t arrowPos = fd.find("->");
    string lhs = fd.substr(0, arrowPos - 1);
    string rhs = fd.substr(arrowPos + 2);
    return {lhs, rhs};
}

// Helper function to split a string by delimiter
vector<string> split(const string& str, char delimiter) {
    vector<string> result;
    stringstream ss(str);
    string token;
    while (getline(ss, token, delimiter)) {
        result.push_back(token);
    }
    return result;
}


// Function to check if vector B is a subset of vector A
bool is_subset(const vector<string>& A, const vector<string>& B) {
    unordered_set<string> setA(A.begin(), A.end());  // Convert vector A to a set for efficient lookup
    for (const auto& elem : B) {
        if (setA.find(elem) == setA.end()) {
            return false;  // If any element of B is not found in A, return false
        }
    }
    return true;  // All elements of B are found in A, so B is a subset of A
}


//Different Implementation to check if string is a superkey
bool isSuperKey(const string& lhs, const vector<string>& keys) {
    // Check if `lhs` is sufficient to determine all attributes in the table
    return find(keys.begin(), keys.end(), lhs) != keys.end();
}


vector<Table> convertTo1NF(Table& baseTable) {
    vector<Table> newTables;
    vector<string> multivaluedAttributes;

    // Find multivalued attributes (those with values inside brackets)
    for (const string& attr : baseTable.attributes) {
        for (const string& value : baseTable.Content[attr]) {
            if (value.front() == '[' && value.back() == ']') {
                multivaluedAttributes.push_back(attr);
                break;
            }
        }
    }

    // Create new tables for each multivalued attribute
    for (const string& mvAttr : multivaluedAttributes) {
        vector<string> newAttributes = baseTable.keys;
        newAttributes.push_back(mvAttr);

        unordered_map<string, vector<string>> newData;
        for (const string& key : baseTable.keys) {
            newData[key] = baseTable.Content[key];  // Copy key columns
        }

        newData[mvAttr] = {};  // Initialize the multivalued column

        for (size_t i = 0; i < baseTable.Content[mvAttr].size(); ++i) {
            string mvValue = baseTable.Content[mvAttr][i];
            if (mvValue.front() == '[' && mvValue.back() == ']') {
                mvValue = mvValue.substr(1, mvValue.size() - 2);  // Remove brackets
                stringstream ss(mvValue);
                string token;
                while (getline(ss, token, ',')) {
                    newData[mvAttr].push_back(token);
                }
            }
        }

        // Ensure types are synchronized with the newAttributes
        vector<string> newTypes;
        for (const string& attr : baseTable.keys) {
            // Find the corresponding type for each key attribute
            auto it = find(baseTable.attributes.begin(), baseTable.attributes.end(), attr);
            if (it != baseTable.attributes.end()) {
                size_t index = distance(baseTable.attributes.begin(), it);
                newTypes.push_back(baseTable.DataType[index]);
            }
        }

        // Add type for the multivalued attribute (assuming it is a string for now)
        newTypes.push_back("VARCHAR(255)");  // Assuming the multivalued attribute is a string

        // Create the new table
        Table newTable(newAttributes, {}, baseTable.keys, newData, newTypes);
        newTables.push_back(newTable);
    }
    // Remove multivalued attributes from the original table
    Table::drop_attributes(baseTable, multivaluedAttributes);
    baseTable.DataType.pop_back();
    newTables.push_back(baseTable);

    return newTables;
}

vector<Table> convertTo2NF(Table& baseTable) {
    // First normalize to 1NF
    vector<Table> tablesIn1NF = convertTo1NF(baseTable);

    // Process each table produced by 1NF
    vector<Table> tablesIn2NF;

    for (auto& table : tablesIn1NF) {
        vector<string> partialDependentAttributes;
        vector<string> partialKeys;

        // Identify partial dependencies
        for (const string& attr : table.attributes) {
            if (find(table.keys.begin(), table.keys.end(), attr) == table.keys.end()) {

                // Check for partial functional dependencies
                for (const string& key : table.keys) {
                    // If an attribute depends on part of a key (but not the whole key), it's a partial dependency
                    for(int i = 0; i < baseTable.FDs.size(); i++){
                      string partial = key + " -> " + attr;
                      if ((baseTable.FDs[i] == partial) && key != wholekey){
                        partialDependentAttributes.push_back(attr);
                        partialKeys.push_back(key);
                        break;
                      }
                    }
                    /*if (find(baseTable.fundamentalDep.begin(), baseTable.fundamentalDep.end(), trim(key) + " -> " + trim(attr)) != baseTable.fundamentalDep.end()){
                        cout << attr;
                        partialDependentAttributes.push_back(attr);
                        partialKeys.push_back(key);
                        break;
                    }*/
                }
            }
        }

        // Create a separate table for each partial dependency
        if (!partialDependentAttributes.empty()) {
            // Create a new table with the partial keys and dependent attributes
            vector<string> newAttributes = partialKeys;
            newAttributes.insert(newAttributes.end(), partialDependentAttributes.begin(), partialDependentAttributes.end());

            unordered_map<string, vector<string>> newData;
            for (const string& key : partialKeys) {
                newData[key] = table.Content[key];  // Copy partial key data
            }

            for (const string& attr : partialDependentAttributes) {
                newData[attr] = table.Content[attr];  // Copy dependent attribute data
            }

            // Create a new table and push it to the 2NF tables list
            vector<string> newTypes;
            for (const string& attr : newAttributes) {
                auto it = find(table.attributes.begin(), table.attributes.end(), attr);
                if (it != table.attributes.end()) {
                    size_t index = distance(table.attributes.begin(), it);
                    newTypes.push_back(table.DataType[index]);
                }
            }
            Table newTable(newAttributes, {}, partialKeys, newData, newTypes);
            tablesIn2NF.push_back(newTable);

            // Remove the partial dependent attributes from the original table
            Table::drop_attributes(table, partialDependentAttributes);
        }

        // Push the updated base table (after removing partial dependencies) to the 2NF tables list
        tablesIn2NF.push_back(table);
    }

    return tablesIn2NF;
}

// Check if a functional dependency is transitive
bool isTransitiveFD(const string& lhs, const string& rhs, const vector<string>& keys) {
    // A transitive FD exists if the LHS is not a key and the RHS is not a key
    return (find(keys.begin(), keys.end(), lhs) == keys.end() &&
            find(keys.begin(), keys.end(), rhs) == keys.end());
}

vector<Table> convertTo3NF(Table& baseTable) {
    // Normalize to 2NF first
    vector<Table> tablesIn2NF = convertTo2NF(baseTable);

    // Store the 3NF tables
    vector<Table> tablesIn3NF;
    // Process each table in 2NF to ensure 3NF
    for (auto& table : tablesIn2NF) {
        vector<string> transitiveDependentAttributes;
        vector<string> nonPrimeKeys;

        // Identify transitive dependencies
        for (const string& attr : table.attributes) {
            if (find(table.keys.begin(), table.keys.end(), attr) == table.keys.end()) {
                for (const string& nonKey : table.attributes) {
                    if (nonKey != attr && find(table.keys.begin(), table.keys.end(), nonKey) == table.keys.end()) {
                        string transitiveDependency = nonKey + " -> " + attr;

                        // Check if a transitive functional dependency exists
                        if (find(baseTable.FDs.begin(), baseTable.FDs.end(), transitiveDependency) != baseTable.FDs.end()) {
                            transitiveDependentAttributes.push_back(attr);
                            nonPrimeKeys.push_back(nonKey);
                            break;
                        }
                    }
                }
            }
        }

        // Create separate tables for each transitive dependency
        if (!transitiveDependentAttributes.empty()) {
            // Define the new table with non-prime keys and transitive dependent attributes
            vector<string> newAttributes = nonPrimeKeys;
            newAttributes.insert(newAttributes.end(), transitiveDependentAttributes.begin(), transitiveDependentAttributes.end());

            unordered_map<string, vector<string>> newData;
            for (const string& nonKey : nonPrimeKeys) {
                newData[nonKey] = table.Content[nonKey];  // Copy non-prime key data
            }

            for (const string& attr : transitiveDependentAttributes) {
                newData[attr] = table.Content[attr];  // Copy transitive dependent attribute data
            }

            // Determine types for the new table
            vector<string> newTypes;
            for (const string& attr : newAttributes) {
                auto it = find(table.attributes.begin(), table.attributes.end(), attr);
                if (it != table.attributes.end()) {
                    size_t index = distance(table.attributes.begin(), it);
                    newTypes.push_back(table.DataType[index]);
                }
            }

            // Create the new table for transitive dependencies
            Table newTable(newAttributes, {}, nonPrimeKeys, newData, newTypes);
            tablesIn3NF.push_back(newTable);

            // Remove transitive dependent attributes from the original table
            Table::drop_attributes(table, transitiveDependentAttributes);
        }

        // Add the updated table (after removing transitive dependencies) to 3NF tables list
        tablesIn3NF.push_back(table);
    }

    return tablesIn3NF;
}


// Function to parse a functional dependency (returns LHS and RHS as vectors)
pair<vector<string>, vector<string>> parseFD(string fd) {
    string LHS = fd.substr(0, fd.find("->"));
    string RHS = fd.substr(fd.find("->") + 2);

    vector<string> parsedLHS, parsedRHS;
    
    // Parse LHS
    stringstream commaSplitLHS(LHS);
    while (commaSplitLHS.good()) {
        string data;
        getline(commaSplitLHS, data, ',');
        parsedLHS.push_back(data);
    }

    // Parse RHS
    stringstream commaSplitRHS(RHS);
    while (commaSplitRHS.good()) {
        string data;
        getline(commaSplitRHS, data, ',');
        parsedRHS.push_back(data);
    }

    return { parsedLHS, parsedRHS };
}

// Function to check if LHS is a superkey
bool isSuperkey(const vector<string>& lhs, const vector<string>& keys) {
    return is_subset(lhs, keys) || is_subset(keys, lhs);
}

vector<Table> convertToBCNF(Table& baseTable) {
    // Ensure the table is in 3NF first
    vector<Table> tables3NF = convertTo3NF(baseTable);
    vector<Table> tablesBCNF = tables3NF;  // Start with the 3NF tables

    for (Table& table : tables3NF) {
        vector<string> newFDs;

        // Process each functional dependency
        for (const auto& fd : baseTable.FDs) {
            auto [lhs, rhs] = parseFD(fd);

            // Only proceed if both lhs and rhs attributes are in the table
            bool hasAllLhs = all_of(lhs.begin(), lhs.end(), [&](const string& attr) {
                return find(table.attributes.begin(), table.attributes.end(), attr) != table.attributes.end();
            });
            bool hasAllRhs = all_of(rhs.begin(), rhs.end(), [&](const string& attr) {
                return find(table.attributes.begin(), table.attributes.end(), attr) != table.attributes.end();
            });

            if (hasAllLhs && hasAllRhs && !isSuperkey(lhs, table.keys)) {
                // Create a new table with LHS as the key and LHS+RHS as attributes
                vector<string> newAttributes = lhs;
                newAttributes.insert(newAttributes.end(), rhs.begin(), rhs.end());

                unordered_map<string, vector<string>> newData;
                for (const auto& attr : newAttributes) {
                    if (table.Content.count(attr)) {  // Ensure attribute exists in data
                        newData[attr] = table.Content[attr];  // Copy relevant columns
                    }
                }

                // Determine data types for the new table
                vector<string> newTypes;
                for (const string& attr : newAttributes) {
                    auto it = find(table.attributes.begin(), table.attributes.end(), attr);
                    if (it != table.attributes.end()) {
                        size_t index = distance(table.attributes.begin(), it);
                        newTypes.push_back(table.DataType[index]);
                    }
                }

                // Create the new table
                Table newTable(newAttributes, {}, lhs, newData, newTypes);
                tablesBCNF.push_back(newTable);

                // Remove RHS attributes from the original table if they exist
                for (const string& attr : rhs) {
                    if (find(table.attributes.begin(), table.attributes.end(), attr) != table.attributes.end()) {
                        Table::drop_attributes(table, {attr});
                    }
                }
            } else {
                // If LHS is a superkey or attributes are missing, retain the functional dependency
                newFDs.push_back(fd);
            }
        }

        // Update the fundamental dependencies of the original table
        table.FDs = newFDs;
    }

    return tablesBCNF;
}
/*
// Helper function to check for multi-valued join dependency
bool isMultiValuedJoinDependency(Table& table, const string& key, const string& attr) {
    unordered_map<string, unordered_set<string>> valuePairs;

    // Check for independent sets of values for `attr` per unique value of `key`
    for (size_t i = 0; i < table.data[key].size(); ++i) {
        string keyVal = table.data[key][i];
        string attrVal = table.data[attr][i];
        valuePairs[keyVal].insert(attrVal);
    }

    // Check if attr values vary independently of key values
    for (const auto& [keyVal, values] : valuePairs) {
        if (values.size() > 1) {
            return true;  // Found MVD for `key ->> attr`
        }
    }
    return false;
}

vector<Table> convertTo4NF(Table& baseTable) {
    // Step 1: Convert the table to BCNF
    vector<Table> tablesBCNF = convertToBCNF(baseTable);
    vector<Table> tables4NF;

    // Step 2: For each table in BCNF, check for multi-valued join dependencies
    for (Table& table : tablesBCNF) {
        bool mvdFound = false;

        // Check if there is a key in the table that causes a multi-valued join dependency
        for (const auto& key : table.keys) {
            vector<string> dependentAttributes;

            // Identify attributes that might be independently reliant on the key
            for (const auto& attr : table.attributes) {
                if (attr != key && isMultiValuedJoinDependency(table, key, attr)) {
                    dependentAttributes.push_back(attr);
                }
            }

            // If we find dependent attributes, create new tables for each MVD set
            if (!dependentAttributes.empty()) {
                mvdFound = true;

                for (const auto& dependentAttr : dependentAttributes) {
                    // Create a new table with the key and the dependent attribute
                    vector<string> newAttributes = {key, dependentAttr};
                    unordered_map<string, vector<string>> newData;

                    // Populate new table with unique pairs of (key, dependent attribute)
                    for (size_t i = 0; i < table.data[key].size(); ++i) {
                        newData[key].push_back(table.data[key][i]);
                        newData[dependentAttr].push_back(table.data[dependentAttr][i]);
                    }

                    // Get data types for new attributes
                    vector<string> newTypes;
                    for (const auto& attr : newAttributes) {
                        auto it = find(table.attributes.begin(), table.attributes.end(), attr);
                        if (it != table.attributes.end()) {
                            size_t index = distance(table.attributes.begin(), it);
                            newTypes.push_back(table.types[index]);
                        }
                    }

                    // Create the new table and add to 4NF tables
                    Table newTable(newAttributes, {}, {key}, newData, newTypes);
                    tables4NF.push_back(newTable);

                    // Remove the dependent attribute from the original table to resolve the MVD
                    Table::remove_columns(table, {dependentAttr});
                }
            }
        }

        // If no MVDs are found, keep the table as it is
        if (!mvdFound) {
            tables4NF.push_back(table);
        }
    }

    return tables4NF;
}
*/

vector<Table> convertTo4NF(Table& baseTable) {
    // Step 1: Convert the table to BCNF
    vector<Table> tablesBCNF = convertToBCNF(baseTable);
    vector<Table> tables4NF;

    // Step 2: For each table in BCNF, handle explicit multi-valued dependencies
    for (Table& table : tablesBCNF) {
        bool mvdFound = false;

        vector<string> newFDs;
        for (const auto& fd : table.FDs) {
            auto [lhs, rhs] = parseFD3(fd);

            // Check if the dependency is a multi-valued dependency (MVD) with a '|'
            if (rhs.find('|') != string::npos) {
                mvdFound = true;

                // Split the RHS by the '|' to get the two independently varying attributes
                auto mvdAttributes = split(rhs, '|');
                string attr1 = trim(mvdAttributes[0]);
                string attr2 = trim(mvdAttributes[1]);

                // Create two new tables for each attribute paired with the key
                vector<string> attributes1 = {lhs, attr1};
                vector<string> attributes2 = {lhs, attr2};

                unordered_map<string, vector<string>> newData1;
                unordered_map<string, vector<string>> newData2;

                // Populate new tables with unique pairs of (lhs, attr1) and (lhs, attr2)
                for (size_t i = 0; i < table.Content[lhs].size(); ++i) {
                    newData1[lhs].push_back(table.Content[lhs][i]);
                    newData1[attr1].push_back(table.Content[attr1][i]);

                    newData2[lhs].push_back(table.Content[lhs][i]);
                    newData2[attr2].push_back(table.Content[attr2][i]);
                }
                // Get data types for new attributes for each new table
                vector<string> types1, types2;
                for (const auto& attr : attributes1) {
                    auto it = find(table.attributes.begin(), table.attributes.end(), attr);
                    if (it != table.attributes.end()) {
                        size_t index = distance(table.attributes.begin(), it);
                        types1.push_back(table.DataType[index]);
                    }
                }
                for (const auto& attr : attributes2) {
                    auto it = find(table.attributes.begin(), table.attributes.end(), attr);
                    if (it != table.attributes.end()) {
                        size_t index = distance(table.attributes.begin(), it);
                        types2.push_back(table.DataType[index]);
                    }
                }

                // Create the new tables and add to 4NF tables
                Table newTable1(attributes1, {}, {lhs}, newData1, types1);
                Table newTable2(attributes2, {}, {lhs}, newData2, types2);
                tables4NF.push_back(newTable1);
                tables4NF.push_back(newTable2);

                // Remove MVD attributes from the original table
                Table::drop_attributes(table, {attr1, attr2});
            } else {
                // If not an MVD, retain the functional dependency
                newFDs.push_back(fd);
            }
        }

        // Update fundamental dependencies for the table after removing MVDs
        table.FDs = newFDs;

        // Add the original table if it wasn't decomposed for MVDs
        if (!mvdFound || !table.attributes.empty()) {
            tables4NF.push_back(table);
        }
    }

    return tables4NF;
}

vector<Table> convertTo5NF(Table& baseTable, const vector<vector<string>>& joinDependencies) {
    // Get tables in 4NF
    vector<Table> tables4NF = convertTo4NF(baseTable);
    vector<Table> tables5NF;

    for (Table& table : tables4NF) {
        unordered_map<string, vector<string>> data = table.Content;

        for (const auto& jd : joinDependencies) {
            if (hasJoinDependency(jd, table.keys, data)) {
                vector<string> jdAttributes = jd;
                unordered_map<string, vector<string>> jdData;

                // Create data projections for this join dependency
                for (const auto& attr : jdAttributes) {
                    if (data.find(attr) != data.end()) {
                        jdData[attr] = data[attr];
                    }
                }

                // Collect types for the join dependency attributes
                vector<string> jdTypes;
                for (const auto& attr : jdAttributes) {
                    jdTypes.push_back(determineDataType(baseTable.Content[attr][0]));
                }

                // Remove join dependency attributes from the base table
                for (const auto& attr : jdAttributes) {
                    data.erase(attr);
                }

                // Create new table from join dependency projection with correct types
                Table newTable(jdAttributes, {}, table.keys, jdData, jdTypes);
                tables5NF.push_back(newTable);
            }
        }

        // Add the remaining base table if attributes remain after decomposition
        if (!data.empty()) {
            vector<string> remainingAttributes;
            vector<string> remainingTypes;

            for (const auto& [attr, values] : data) {
                remainingAttributes.push_back(attr);

                // Collect the type for each remaining attribute
                auto it = find(table.attributes.begin(), table.attributes.end(), attr);
                if (it != table.attributes.end()) {
                    size_t index = distance(table.attributes.begin(), it);
                    remainingTypes.push_back(table.DataType[index]);
                }
            }

            // Create the remaining table with correct types
            Table remainingTable(remainingAttributes, {}, table.keys, data, remainingTypes);
            tables5NF.push_back(remainingTable);
        }
    }

    return tables5NF;
}


bool isIn1nf(Table inputTable)
{
  for (const auto& [attribute, values] : inputTable.Content) {
        for (const auto& value : values) {
            if(value.front() == '[' && value.back() == ']')
              return false;
        }
    }
  if(inputTable.keys.empty())
    return false;
  return true;
}

bool isIn2nf(const Table& table) {
    // If there is only one key, partial dependency isn't possible
    if(!isIn1nf(table))
      return false;
    
    if (table.keys.size() == 1) {
        return true;
    }

    // Check each attribute for partial dependencies
    for (const string& attr : table.attributes) {
        // Skip key attributes
        if (find(table.keys.begin(), table.keys.end(), attr) != table.keys.end()) {
            continue;
        }

        // Check if the attribute depends on part of a composite key
        for (const string& key : table.keys) {
            // Construct the partial dependency
            string partialDep = key + " -> " + attr;

            // If a partial dependency exists, the table is not in 2NF
            if (find(table.FDs.begin(), table.FDs.end(), partialDep) != table.FDs.end()) {
                return false;
            }
        }
    }

    return true;
}

bool isIn3nf(const Table& table) {
    // Iterate over each functional dependency in the table
    if(!isIn2nf(table))
        return false;
    for (const auto& fd : table.FDs) {
        // Parse left-hand side (LHS) and right-hand side (RHS) of the functional dependency
        string lhs = fd.substr(0, fd.find("->"));
        string rhs = fd.substr(fd.find("->") + 2);

        // Check if RHS is a key attribute
        if (find(table.keys.begin(), table.keys.end(), rhs) != table.keys.end()) {
            continue;  // If RHS is a key attribute, it meets 3NF for this dependency
        }

        // Check if LHS is a superkey
        bool lhsIsSuperkey = isSuperKey(lhs, table.keys);

        // Check if RHS is a non-prime attribute (not part of any key)
        bool rhsIsNonPrime = (find(table.keys.begin(), table.keys.end(), rhs) == table.keys.end());

        // Check 3NF conditions: either LHS is a superkey or RHS is a prime attribute
        if (!lhsIsSuperkey && rhsIsNonPrime) {
            return false;  // A transitive dependency found, so not in 3NF
        }
    }
    return true;  // No transitive dependencies found, so the table is in 3NF
}

bool isInBCNF(const Table& table) {
    if(!isIn3nf(table))
        return false;
    // Check each functional dependency in the table
    for (const auto& fd : table.FDs) {
        auto [lhs, rhs] = parseFD(fd);

        // Check if LHS is a superkey
        if (!isSuperkey(lhs, table.keys)) {
            // If LHS is not a superkey, the table is not in BCNF
            return false;
        }
    }
    // If all functional dependencies have LHS as a superkey, the table is in BCNF
    return true;
}

bool isIn4nf(const Table& table) {
    if(!isInBCNF(table))
        return false;
    // Iterate through each functional dependency
    for (const auto& fd : table.FDs) {
        // Parse FD into LHS and RHS
        string lhs = fd.substr(0, fd.find("->"));
        string rhs = fd.substr(fd.find("->") + 2);

        // Separate LHS and RHS attributes
        vector<string> lhsAttrs = parseFD2(lhs);
        vector<string> rhsAttrs = parseFD2(rhs);

        bool isMVD = true;
        for (const string& attr : table.attributes) {
            if (find(lhsAttrs.begin(), lhsAttrs.end(), attr) == lhsAttrs.end() &&
                find(rhsAttrs.begin(), rhsAttrs.end(), attr) == rhsAttrs.end()) {
                // Check for independent values of RHS; if there's dependency, it's not an MVD
                if (!isFD(lhsAttrs, attr, table.Content)) {
                    isMVD = false;
                    break;
                }
            }
        }

        // If an MVD exists that isn't implied by the keys, the table is not in 4NF
        if (isMVD && !isSuperkey(lhsAttrs, table.keys)) {
            return false;
        }
    }

    // If no non-key MVDs were found, the table is in 4NF
    return true;
}

bool isIn5nf(const Table& table, const vector<vector<string>>& joinDependencies) {
    if(!isIn4nf(table))
        return false;
    unordered_map<string, vector<string>> data = table.Content;

    for (const auto& jd : joinDependencies) {
        if (hasJoinDependency(jd, table.keys, data)) {
            vector<string> jdAttributes = jd;
            
            // Check if the join dependency can be decomposed without including a candidate key
            unordered_set<string> jdAttrSet(jdAttributes.begin(), jdAttributes.end());
            bool includesCandidateKey = false;
            for (const auto& key : table.keys) {
                if (jdAttrSet.find(key) != jdAttrSet.end()) {
                    includesCandidateKey = true;
                    break;
                }
            }

            // If the join dependency doesn't include a candidate key, it violates 5NF
            if (!includesCandidateKey) {
                return false;
            }
        }
    }

    // If no non-trivial join dependencies were found, the table is in 5NF
    return true;
}


int Table::Total = 0;

int main() {
    const string file = "Table.csv";
    const string FuncDeps = "FD.txt";

    Table testTable = loadTableData(file, FuncDeps);
    string input;
    cout << "Do you want to check what NF the table is in (CHECK) or normalize the input (NORM)?" << endl;
    cin >> input;
    if (input == "CHECK") {
        if (!isIn1nf(testTable)){
            cout << "The highest normal form is Serial NF" << endl;
        }
        else if(!isIn2nf(testTable)){
            cout << "The highest normal form is 1NF" << endl;
        }
        else if(!isIn3nf(testTable)){
            cout << "The highest normal form is 2NF" << endl;
        }
        else if(!isInBCNF(testTable)){
            cout << "The highest normal form is 3NF" << endl;
        }
        else if(!isIn4nf(testTable)){
            cout << "The highest normal form is BCNF" << endl;
        }
        else if(!isIn5nf(testTable, {})){
            cout << "The highest normal form is 4NF" << endl;
        }
        else{
            cout << "The highest normal form is 5NF" << endl;
        }
    }
    else if(input == "NORM"){
        cout << "Please input the normal form you want to convert to (1NF, 2NF, 3NF, BCNF, 4NF, 5NF)" << endl;
        cin >> input;
        if (input == "1NF") {
            vector<Table> results = convertTo1NF(testTable);
            generateOutput(results);
        } else if (input == "2NF") {
            vector<Table> results = convertTo2NF(testTable);
            generateOutput(results);
        } else if (input == "3NF") {
            vector<Table> results = convertTo3NF(testTable);
            generateOutput(results);
        } else if (input == "BCNF") {
            vector<Table> results = convertToBCNF(testTable);
            generateOutput(results);
        } else if (input == "4NF") {
            vector<Table> results = convertTo4NF(testTable);
            generateOutput(results);
        } else if (input == "5NF") {
            cout << "Please input the join dependencies (e.g. A,B;C,D)" << endl;
            cin >> input;
            vector<vector<string>> joinDependencies;
            stringstream ss(input);
            string pair;
            while (getline(ss, pair, ';')) {
                vector<string> jd = split(pair, ',');
                joinDependencies.push_back(jd);
            }
            vector<Table> results = convertTo5NF(testTable, joinDependencies);
            generateOutput(results);
        } else {
            cout << "Invalid input" << endl;
        }
    }
    else{
        cout << "Invalid input" << endl;
    }
    return 0;
}
