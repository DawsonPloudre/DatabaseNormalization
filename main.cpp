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

    //Keep track of how many tables we have for when we need to output the tables
    static int Total;

    string name;
    vector<string> FDs, attributes, DataType, keys;
    unordered_map<string, vector<string>> Content;


    //Constructor for making new tables
    Table(const vector<string>& tableAttributes,const vector<string>& tableKeys, const vector<string>& fd, unordered_map<string, vector<string>> ConstructorContent, const vector<string> ConstructorType)
        : name("Table" + to_string(Total++)), attributes(tableAttributes), keys(tableKeys), FDs(fd), Content(ConstructorContent), DataType(ConstructorType) {}

};

//This should remove columns when needed
void EraseCols(Table& table, const vector<string>& attributesToRemove) {
    for (const string& attribute : attributesToRemove) {
        auto it = find(table.attributes.begin(), table.attributes.end(), attribute);
        if (it != table.attributes.end()) {
            size_t index = distance(table.attributes.begin(), it);
            table.attributes.erase(table.attributes.begin() + index);
            table.DataType.erase(table.DataType.begin() + index);
            table.Content.erase(attribute);
        }
    }
}

string identifyDataType(const string& field) {
    if (field.empty()) {
        return "EMPTY";
    }

    // Check for integer type
    bool allDigits = all_of(field.begin(), field.end(), ::isdigit);
    if (allDigits) {
        return "INT";
    }

    // Check for date format
    istringstream dateStream(field);
    int m, d, y;
    char sep;
    dateStream >> m >> sep >> d >> sep >> y;
    if (sep == '/' && dateStream) {
        return "DATE";
    }

    // Default to VARCHAR(255)
    return "VARCHAR(255)";
}


Table loadTableData(const string& dataFile, const string& depFile) {
    ifstream fdFile(depFile);
    string line;
    vector<string> fdList;

    // Read Functional Dependencies from File
    if (fdFile.is_open()) {
        while (getline(fdFile, line)) {
            fdList.push_back(line);
        }
        fdFile.close();
    }

    // Input and Parse Keys
    string keysInput;
    cout << "\nEnter the primary keys, separated by commas: ";
    cin >> keysInput;
    stringstream keyStream(keysInput);
    string singleKey;
    vector<string> primaryKeys;
    while (getline(keyStream, singleKey, ',')) {
        primaryKeys.push_back(singleKey);
    }

    // Parse Attributes from the First Line of the File
    ifstream dataFileStream(dataFile);
    getline(dataFileStream, line);
    stringstream attributeStream(line);
    string column;
    vector<string> attributes;
    while (getline(attributeStream, column, ',')) {
        attributes.push_back(column);
    }

    // Determine Data Types for Each Column
    vector<string> columnTypes;
    getline(dataFileStream, line);  // Skip header line
    getline(dataFileStream, line);  // Read the first row to determine types
    stringstream typeStream(line);
    string cellValue;

    while (getline(typeStream, cellValue, ',')) {
        string inferredType = identifyDataType(cellValue);
        columnTypes.push_back(inferredType);
    }
    dataFileStream.close();

    // Create Table with Attributes and Empty Content
    Table newTable(attributes, fdList, primaryKeys, {}, columnTypes);

    // Reopen Data File to Parse Each Row and Populate Content
    ifstream dataFileReload(dataFile);
    getline(dataFileReload, line);  // Skip header
    while (getline(dataFileReload, line)) {
        stringstream rowStream(line);
        string cell;
        vector<string> rowData;
        bool inQuotes = false;
        string temp = "";

        while (getline(rowStream, cell, ',')) {
            if (!inQuotes) {
                if (cell.front() == '\"' && cell.back() == '\"') {
                    rowData.push_back(cell.substr(1, cell.size() - 2));
                } else if (cell.front() == '\"') {
                    inQuotes = true;
                    temp = cell;
                } else {
                    rowData.push_back(cell);
                }
            } else {
                temp += "," + cell;
                if (cell.back() == '\"') {
                    inQuotes = false;
                    rowData.push_back(temp.substr(1, temp.size() - 2));
                }
            }
        }

        if (rowData.size() != attributes.size()) {
            cerr << "Warning: Row has incorrect number of columns, skipping row." << endl;
            continue;
        }

        // Store Row Data in the Table's Content
        for (size_t i = 0; i < attributes.size(); ++i) {
            newTable.Content[attributes[i]].push_back(rowData[i]);
        }
    }
    dataFileReload.close();

    return newTable;
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    return (first == string::npos || last == string::npos) ? "" : str.substr(first, last - first + 1);
}

// Check if a functional dependency is transitive
bool isTransitiveFD(const string& lhs, const string& rhs, const vector<string>& keys) {
    // A transitive FD exists if the LHS is not a key and the RHS is not a key
    return (find(keys.begin(), keys.end(), lhs) == keys.end() &&
            find(keys.begin(), keys.end(), rhs) == keys.end());
}

void generateOutput(const vector<Table>& results) {
    ofstream sqlFile("output.sql");
    
    if (!sqlFile) {
        cerr << "Error: Could not open the output file." << endl;
        return;
    }

    for (const auto& tbl : results) {
        sqlFile << "CREATE TABLE " << tbl.name << " (\n";

        // Confirm matching attribute and type sizes
        if (tbl.attributes.size() != tbl.DataType.size()) {
            cerr << "Error: Attribute-type mismatch in table " << tbl.name << endl;
            continue;
        }

        // Output columns with data types
        auto attrIt = tbl.attributes.begin();
        auto typeIt = tbl.DataType.begin();
        while (attrIt != tbl.attributes.end() && typeIt != tbl.DataType.end()) {
            sqlFile << "\t" << *attrIt << " " << *typeIt;

            if (next(attrIt) != tbl.attributes.end()) {
                sqlFile << ",\n";
            } else {
                sqlFile << "\n";
            }
            ++attrIt;
            ++typeIt;
        }

        sqlFile << ");\n\n";
    }
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

pair<string, vector<string>> parseMVD(const string& mvd) {
    string lhs = mvd.substr(0, mvd.find("->"));
    string rhs_part = mvd.substr(mvd.find("->") + 2);
    vector<string> rhs;

    stringstream pipeSplit(rhs_part);
    while (pipeSplit.good()) {
        string attribute;
        getline(pipeSplit, attribute, '|');
        rhs.push_back(attribute);
    }

    return {lhs, rhs};
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


// Check if an attribute belongs to a set of keys
bool is_in(const string& element, const unordered_set<string>& key_set) {
    return key_set.find(element) != key_set.end();
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
            return false;  // If candidate key is part of the join, it’s not a true join dependency
        }
    }
    return true;
}

vector<Table> convertTo1NF(Table& baseTable) {
    vector<Table> newTables;
    vector<string> multivaluedAttributes;

    // Identify multivalued attributes by checking for values in brackets
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
        // Set up new attributes with keys and the multivalued attribute
        vector<string> newAttributes = baseTable.keys;
        newAttributes.push_back(mvAttr);

        unordered_map<string, vector<string>> newData;
        for (const string& key : baseTable.keys) {
            newData[key] = baseTable.Content[key];  // Copy key columns
        }

        // Initialize the multivalued column
        newData[mvAttr] = {};

        // Process and flatten the multivalued entries
        for (const string& mvValue : baseTable.Content[mvAttr]) {
            if (mvValue.front() == '[' && mvValue.back() == ']') {
                // Remove brackets and split by comma
                stringstream ss(mvValue.substr(1, mvValue.size() - 2));
                string token;
                while (getline(ss, token, ',')) {
                    newData[mvAttr].push_back(token);
                }
            }
        }

        // Synchronize types for each attribute in newAttributes
        vector<string> newTypes;
        for (const string& attr : newAttributes) {
            // If attr is a key, get its type from baseTable
            auto it = find(baseTable.attributes.begin(), baseTable.attributes.end(), attr);
            if (it != baseTable.attributes.end()) {
                size_t index = distance(baseTable.attributes.begin(), it);
                newTypes.push_back(baseTable.DataType[index]);
            } else if (attr == mvAttr) {
                // For the multivalued attribute, assign VARCHAR(255) as default
                newTypes.push_back("VARCHAR(255)");
            }
        }

        // Create the new table with aligned attributes and types
        Table newTable(newAttributes, baseTable.keys, {}, newData, newTypes);
        newTables.push_back(newTable);
    }

    // Remove multivalued attributes from the original table
    EraseCols(baseTable, multivaluedAttributes);

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
                    cout << baseTable.FDs.at(0) << endl;
                    for(int i = 0; i < baseTable.FDs.size(); i++){
                      string partial = key + " -> " + attr;
                      if ((baseTable.FDs[i] == partial) && key != wholekey){
                        cout << attr;
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
            EraseCols(table, partialDependentAttributes);
        }

        // Push the updated base table (after removing partial dependencies) to the 2NF tables list
        tablesIn2NF.push_back(table);
    }

    return tablesIn2NF;
}

// Function to normalize a table into 3NF
vector<Table> convertTo3NF(Table& baseTable) {
    // Ensure the table is in 2NF first
    vector<Table> tables2NF = convertTo2NF(baseTable);
    vector<Table> tables3NF = tables2NF;  // Start with the 2NF tables

    for (Table& table : tables2NF) {
        vector<string> nonKeys;
        vector<string> newFDs;

        // Identify non-key attributes
        for (const auto& attr : table.attributes) {
            if (find(table.keys.begin(), table.keys.end(), attr) == table.keys.end()) {
                nonKeys.push_back(attr);
            }
        }

        // Check for transitive dependencies
        for (const auto& fd : table.FDs) {
            string lhs = fd.substr(0, fd.find("->"));
            string rhs = fd.substr(fd.find("->") + 2);

            // If the functional dependency is transitive, create a new table for it
            if (isTransitiveFD(lhs, rhs, table.keys)) {
                vector<string> transitiveKeys = { lhs };  // LHS of FD becomes the key of the new table
                vector<string> newAttributes = { lhs, rhs };
                unordered_map<string, vector<string>> newData;

                // Collect the data for the new table
                newData[lhs] = table.Content[lhs];
                newData[rhs] = table.Content[rhs];

                // Determine the data types
                vector<string> newDataTypes = {
                    identifyDataType(newData[lhs][0]),
                    identifyDataType(newData[rhs][0])
                };

                // Create the new table
                Table newTable(newAttributes, { fd }, transitiveKeys, newData, newDataTypes);
                tables3NF.push_back(newTable);

                // Remove the transitive dependency from the original table
                EraseCols(table, { rhs });
            } else {
                newFDs.push_back(fd);  // Retain FDs that are not transitive
            }
        }

        // Update the original table's fundamental dependencies
        table.FDs = newFDs;
    }

    return tables3NF;
}

// Function to normalize a table into BCNF
vector<Table> convertToBCNF(Table& baseTable) {
    // Ensure the table is in 3NF first
    vector<Table> tables3NF = convertTo3NF(baseTable);
    vector<Table> tablesBCNF = tables3NF;  // Start with the 3NF tables

    for (Table& table : tables3NF) {
        vector<string> newFDs;

        // Process each functional dependency
        for (const auto& fd : table.FDs) {
            auto [lhs, rhs] = parseFD(fd);

            // If LHS is not a superkey, create a new table for the FD
            if (!isSuperkey(lhs, table.keys)) {
                // Create a new table with LHS as the key and LHS+RHS as attributes
                vector<string> newAttributes = lhs;
                newAttributes.insert(newAttributes.end(), rhs.begin(), rhs.end());

                unordered_map<string, vector<string>> newData;
                for (const auto& attr : newAttributes) {
                    newData[attr] = table.Content[attr];  // Copy relevant columns
                }

                // Determine data types for the new table
                vector<string> newDataTypes;
                for (const auto& attr : newAttributes) {
                    newDataTypes.push_back(identifyDataType(newData[attr][0]));
                }

                // Create the new table
                Table newTable(newAttributes, { fd }, lhs, newData, newDataTypes);
                tablesBCNF.push_back(newTable);

                // Remove RHS attributes from the original table
                EraseCols(table, rhs);
            } else {
                // If LHS is a superkey, retain the functional dependency
                newFDs.push_back(fd);
            }
        }

        // Update the fundamental dependencies of the original table
        table.FDs = newFDs;
    }

    return tablesBCNF;
}

// Convert to 4NF by normalizing tables returned from convertTo3NF
vector<Table> convertTo4NF(Table& baseTable) {
    // Get tables in 3NF
    vector<Table> tables3NF = convertTo3NF(baseTable);
    vector<Table> tables4NF;

    for (Table& table : tables3NF) {
        // Start with the current 3NF table in the 4NF result set
        tables4NF.push_back(table);

        // Analyze FDs to detect potential MVDs
        for (const auto& fd : table.FDs) {
            // Parse FD into left-hand side (LHS) and right-hand side (RHS)
            string lhs = fd.substr(0, fd.find("->"));
            string rhs = fd.substr(fd.find("->") + 2);

            // Separate LHS and RHS attributes
            vector<string> lhsAttrs = parseFD2(lhs);
            vector<string> rhsAttrs = parseFD2(rhs);

            // Check if this FD behaves like an MVD by seeing if RHS can exist independently of other columns
            bool isMVD = true;
            for (const string& attr : table.attributes) {
                if (find(lhsAttrs.begin(), lhsAttrs.end(), attr) == lhsAttrs.end() &&
                    find(rhsAttrs.begin(), rhsAttrs.end(), attr) == rhsAttrs.end()) {
                    // If there's an attribute not in LHS or RHS that correlates with RHS values, it's not an MVD
                    if (!isFD(lhsAttrs, attr, table.Content)) {
                        isMVD = false;
                        break;
                    }
                }
            }

            if (isMVD) {
                // Construct a new table for the MVD
                vector<string> newAttributes = lhsAttrs;
                newAttributes.insert(newAttributes.end(), rhsAttrs.begin(), rhsAttrs.end());

                unordered_map<string, vector<string>> newData;
                for (const string& attr : newAttributes) {
                    newData[attr] = table.Content[attr];
                }

                // Create the new MVD table
                vector<string> emptyTypes(newAttributes.size(), "");
                Table mvdTable(newAttributes, {}, lhsAttrs, newData, emptyTypes);
                tables4NF.push_back(mvdTable);

                // Remove RHS attributes from the original table to ensure no redundancy
                EraseCols(table, rhsAttrs);
            }
        }
    }

    return tables4NF;
}

// Convert to 5NF by normalizing tables returned from convertTo4NF
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

                // Remove join dependency attributes from the base table
                for (const auto& attr : jdAttributes) {
                    data.erase(attr);
                }

                // Create new table from join dependency projection
                Table newTable(jdAttributes, {}, table.keys, jdData, {});
                tables5NF.push_back(newTable);
            }
        }

        // Add the remaining base table if attributes remain after decomposition
        if (!data.empty()) {
            vector<string> remainingAttributes;
            for (const auto& [attr, values] : data) {
                remainingAttributes.push_back(attr);
            }
            Table remainingTable(remainingAttributes, {}, table.keys, data, {});
            tables5NF.push_back(remainingTable);
        }
    }

    return tables5NF;
}

bool isIn1nf(Table inputTable)
{
  for (const auto& [attribute, values] : inputTable.Content) {
        if(attribute.front() == '"' && attribute.back() == '"')
          return false;
        for (const auto& value : values) {
            if(value.front() == '"' && value.back() == '"')
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

//Different Implementation to check if string is a superkey
bool isSuperKey(const string& lhs, const vector<string>& keys) {
    // Check if `lhs` is sufficient to determine all attributes in the table
    return find(keys.begin(), keys.end(), lhs) != keys.end();
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

  //example join dependencies
  vector<vector<string>> joinDependencies = {
    {"OrderID", "CustomerID", "ProductID"},
    {"OrderID", "CustomerID", "SupplierID"}
  };
    const string file = "Table.csv";
    const string FuncDeps = "FD.txt";

    Table inputTable = loadTableData(file, FuncDeps);
    string input;
    cout << "Do you want to check what NF the table is in? (y/n)" << endl;
    cin >> input;
    if (input == "y") {
        if (isIn5nf(inputTable, joinDependencies)){
            cout << "The highest normal form of the table is 5NF" << endl;
        }
        else if (isIn4nf(inputTable)){
            cout << "The highest normal form of the table is 4NF" << endl;
        }
        else if(isInBCNF(inputTable)){
            cout << "The highest normal form of the table is BCNF" << endl;
        }
        else if (isIn3nf(inputTable)){
            cout << "The highest normal form of the table is 3NF" << endl;
        }
        else if (isIn2nf(inputTable)){
            cout << "The highest normal form of the table is 2NF" << endl;
        }
        else if (isIn1nf(inputTable)){
            cout << "The highest normal form of the table is 1NF" << endl;
        }
        else{
            cout << "The table is in serail normal form" << endl;
        }
    }
    cout << "Please input the normal form you want to convert to (1NF, 2NF, 3NF, BCNF, 4NF, 5NF)" << endl;
    cin >> input;
    if (input == "1NF") {
        vector<Table> results = convertTo1NF(inputTable);
        generateOutput(results);
    } else if (input == "2NF") {
        vector<Table> results = convertTo2NF(inputTable);
        generateOutput(results);
    } else if (input == "3NF") {
        vector<Table> results = convertTo3NF(inputTable);
        generateOutput(results);
    } else if (input == "BCNF") {
        vector<Table> results = convertToBCNF(inputTable);
        generateOutput(results);
    } else if (input == "4NF") {
        vector<Table> results = convertTo4NF(inputTable);
        generateOutput(results);
    } else if (input == "5NF") {
        vector<Table> results = convertTo5NF(inputTable, joinDependencies);
        generateOutput(results);
    } else {
        cout << "Invalid input" << endl;
    }

    return 0;
}
