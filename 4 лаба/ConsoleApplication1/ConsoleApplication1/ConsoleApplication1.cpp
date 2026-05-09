#include <algorithm>
#include <cctype>
#include <clocale>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace std;

struct Token {
    string type;
    string value;
    int line;
    int column;
};

struct ASTNode {
    string name;
    string value;
    int line;
    vector<ASTNode*> children;

    ASTNode(string nodeName, string nodeValue = "", int nodeLine = -1) {
        name = nodeName;
        value = nodeValue;
        line = nodeLine;
    }

    ~ASTNode() {
        for (ASTNode* child : children) {
            delete child;
        }
    }

    void add(ASTNode* child) {
        children.push_back(child);
    }
};

struct PreprocessResult {
    bool ok;
    string code;
    vector<string> errors;
};

bool isKeyword(const string& s) {
    string keywords[] = {
        "using", "namespace", "int", "bool", "return",
        "if", "else", "for", "while"
    };

    for (const string& keyword : keywords) {
        if (s == keyword) {
            return true;
        }
    }

    return false;
}

bool isBoolConst(const string& s) {
    return s == "true" || s == "false";
}

bool isDelimiter(char c) {
    string delimiters = ";,(){}";
    return delimiters.find(c) != string::npos;
}

bool isOneCharOperator(char c) {
    string operators = "#=+-*/<>!";
    return operators.find(c) != string::npos;
}

bool isTwoCharOperator(const string& s) {
    string operators[] = {
        "&&", "||", "<<", ">>", "==", "!=", "<=", ">="
    };

    for (const string& op : operators) {
        if (s == op) {
            return true;
        }
    }

    return false;
}

string trim(const string& line) {
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
        start++;
    }

    size_t end = line.size();
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r')) {
        end--;
    }

    return line.substr(start, end - start);
}

bool checkInvalidCharacters(const string& code, vector<string>& errors) {
    for (int i = 0; i < static_cast<int>(code.length()); i++) {
        unsigned char ch = static_cast<unsigned char>(code[i]);

        if (!(ch == '\n' || ch == '\r' || ch == '\t' || ch >= 32)) {
            errors.push_back("Недопустимый символ в позиции " + to_string(i));
            return false;
        }
    }

    return true;
}

bool checkMultilineComments(const string& code, vector<string>& errors) {
    int balance = 0;

    for (int i = 0; i + 1 < static_cast<int>(code.length()); i++) {
        if (code[i] == '/' && code[i + 1] == '*') {
            balance++;
            i++;
        }
        else if (code[i] == '*' && code[i + 1] == '/') {
            balance--;
            i++;

            if (balance < 0) {
                errors.push_back("Найдено закрытие многострочного комментария без открытия");
                return false;
            }
        }
    }

    if (balance > 0) {
        errors.push_back("Найден незакрытый многострочный комментарий");
        return false;
    }

    return true;
}

string removeComments(const string& code) {
    string result;

    for (int i = 0; i < static_cast<int>(code.size()); i++) {
        if (i + 1 < static_cast<int>(code.size()) && code[i] == '/' && code[i + 1] == '/') {
            while (i < static_cast<int>(code.size()) && code[i] != '\n') {
                i++;
            }

            if (i < static_cast<int>(code.size())) {
                result += '\n';
            }
        }
        else if (i + 1 < static_cast<int>(code.size()) && code[i] == '/' && code[i + 1] == '*') {
            i += 2;

            while (i + 1 < static_cast<int>(code.size()) && !(code[i] == '*' && code[i + 1] == '/')) {
                if (code[i] == '\n') {
                    result += '\n';
                }

                i++;
            }

            i++;
        }
        else {
            result += code[i];
        }
    }

    return result;
}

string removeExtraSpaces(const string& code) {
    stringstream input(code);
    string line;
    string result;

    while (getline(input, line)) {
        string current = trim(line);

        if (!current.empty()) {
            result += current + "\n";
        }
    }

    return result;
}

PreprocessResult preprocess(const string& code) {
    PreprocessResult result;
    result.ok = true;

    if (!checkInvalidCharacters(code, result.errors)) {
        result.ok = false;
        return result;
    }

    if (!checkMultilineComments(code, result.errors)) {
        result.ok = false;
        return result;
    }

    result.code = removeExtraSpaces(removeComments(code));
    return result;
}

vector<Token> lexicalAnalyze(const string& code, vector<string>& errors) {
    vector<Token> tokens;
    int i = 0;
    int line = 1;
    int column = 1;

    auto advance = [&]() {
        if (i < static_cast<int>(code.length())) {
            if (code[i] == '\n') {
                line++;
                column = 1;
            }
            else {
                column++;
            }
            i++;
        }
    };

    while (i < static_cast<int>(code.length())) {
        char c = code[i];

        if (isspace(static_cast<unsigned char>(c))) {
            advance();
        }
        else if (c == '#') {
            int startLine = line;
            int startColumn = column;
            string word = "#";
            advance();

            while (i < static_cast<int>(code.length()) && isalpha(static_cast<unsigned char>(code[i]))) {
                word += code[i];
                advance();
            }

            if (word == "#include") {
                tokens.push_back({ "PREPROCESSOR", word, startLine, startColumn });
            }
            else {
                errors.push_back("Строка " + to_string(startLine) + ": неизвестная директива препроцессора " + word);
            }
        }
        else if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
            int startLine = line;
            int startColumn = column;
            string word;

            while (i < static_cast<int>(code.length()) &&
                (isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_')) {
                word += code[i];
                advance();
            }

            if (isBoolConst(word)) {
                tokens.push_back({ "CONSTANT_BOOL", word, startLine, startColumn });
            }
            else if (isKeyword(word)) {
                tokens.push_back({ "KEYWORD", word, startLine, startColumn });
            }
            else {
                tokens.push_back({ "IDENTIFIER", word, startLine, startColumn });
            }
        }
        else if (isdigit(static_cast<unsigned char>(c))) {
            int startLine = line;
            int startColumn = column;
            string number;
            bool hasError = false;

            while (i < static_cast<int>(code.length()) &&
                (isdigit(static_cast<unsigned char>(code[i])) || code[i] == '.' || isalpha(static_cast<unsigned char>(code[i])))) {
                if (code[i] == '.' || isalpha(static_cast<unsigned char>(code[i]))) {
                    hasError = true;
                }

                number += code[i];
                advance();
            }

            if (hasError) {
                errors.push_back("Строка " + to_string(startLine) + ": ошибка в целочисленной константе " + number);
            }
            else {
                tokens.push_back({ "CONSTANT_INT", number, startLine, startColumn });
            }
        }
        else if (c == '"') {
            int startLine = line;
            int startColumn = column;
            string text;
            text += c;
            advance();

            bool closed = false;

            while (i < static_cast<int>(code.length())) {
                text += code[i];

                if (code[i] == '"') {
                    advance();
                    closed = true;
                    break;
                }

                advance();
            }

            if (closed) {
                tokens.push_back({ "STRING", text, startLine, startColumn });
            }
            else {
                errors.push_back("Строка " + to_string(startLine) + ": незакрытая строковая константа");
            }
        }
        else {
            int startLine = line;
            int startColumn = column;

            if (i + 1 < static_cast<int>(code.length())) {
                string two;
                two += code[i];
                two += code[i + 1];

                if (isTwoCharOperator(two)) {
                    tokens.push_back({ "OPERATOR", two, startLine, startColumn });
                    advance();
                    advance();
                    continue;
                }
            }

            if (isDelimiter(c)) {
                string s;
                s += c;
                tokens.push_back({ "DELIMITER", s, startLine, startColumn });
                advance();
            }
            else if (isOneCharOperator(c)) {
                string s;
                s += c;
                tokens.push_back({ "OPERATOR", s, startLine, startColumn });
                advance();
            }
            else {
                string s;
                s += c;
                errors.push_back("Строка " + to_string(startLine) + ": недопустимый символ " + s);
                advance();
            }
        }
    }

    tokens.push_back({ "EOF", "EOF", line, column });
    return tokens;
}

class Parser {
private:
    vector<Token> tokens;
    int pos;
    bool hasError;

public:
    Parser(const vector<Token>& inputTokens) {
        tokens = inputTokens;
        pos = 0;
        hasError = false;
    }

    ASTNode* parseProgram() {
        ASTNode* program = new ASTNode("Program");

        while (check("PREPROCESSOR", "#include")) {
            program->add(parseInclude());
        }

        while (check("KEYWORD", "using")) {
            program->add(parseUsing());
        }

        while (!check("EOF")) {
            program->add(parseFunction());
        }

        return program;
    }

    bool success() const {
        return !hasError;
    }

private:
    Token current() const {
        if (pos < static_cast<int>(tokens.size())) {
            return tokens[pos];
        }

        return { "EOF", "EOF", -1, -1 };
    }

    bool check(const string& type, const string& value = "") const {
        Token token = current();

        if (token.type != type) {
            return false;
        }

        if (!value.empty() && token.value != value) {
            return false;
        }

        return true;
    }

    bool isType() const {
        return check("KEYWORD", "int") || check("KEYWORD", "bool");
    }

    Token consume(const string& type, const string& value, const string& expected) {
        Token token = current();

        if (check(type, value)) {
            pos++;
            return token;
        }

        printError(expected);

        if (!check("EOF")) {
            pos++;
        }

        return token;
    }

    Token consumeType() {
        Token token = current();

        if (isType()) {
            pos++;
            return token;
        }

        printError("тип данных int или bool");

        if (!check("EOF")) {
            pos++;
        }

        return token;
    }

    void printError(const string& expected) {
        Token token = current();

        cout << "Синтаксическая ошибка: ожидалось " << expected
            << ", найдено " << token.type << " '" << token.value << "'"
            << " в позиции " << token.line << ":" << token.column << endl;

        hasError = true;
    }

    ASTNode* parseInclude() {
        ASTNode* node = new ASTNode("include_directive", "", current().line);

        consume("PREPROCESSOR", "#include", "директива #include");
        consume("OPERATOR", "<", "символ <");

        Token library = consume("IDENTIFIER", "", "имя библиотеки");
        node->add(new ASTNode("library", library.value, library.line));

        consume("OPERATOR", ">", "символ >");

        return node;
    }

    ASTNode* parseUsing() {
        ASTNode* node = new ASTNode("using_directive", "", current().line);

        consume("KEYWORD", "using", "ключевое слово using");
        consume("KEYWORD", "namespace", "ключевое слово namespace");

        Token namespaceName = consume("IDENTIFIER", "", "имя пространства имён");
        node->add(new ASTNode("namespace", namespaceName.value, namespaceName.line));

        consume("DELIMITER", ";", "разделитель ;");

        return node;
    }

    ASTNode* parseFunction() {
        ASTNode* node = new ASTNode("function_decl", "", current().line);

        Token returnType = consumeType();
        node->add(new ASTNode("return_type", returnType.value, returnType.line));

        Token functionName = consume("IDENTIFIER", "", "имя функции");
        node->add(new ASTNode("name", functionName.value, functionName.line));

        consume("DELIMITER", "(", "открывающая скобка (");
        node->add(parseParameters());
        consume("DELIMITER", ")", "закрывающая скобка )");

        node->add(parseBlock("body"));
        return node;
    }

    ASTNode* parseParameters() {
        ASTNode* params = new ASTNode("parameters", "", current().line);

        if (check("DELIMITER", ")")) {
            return params;
        }

        params->add(parseParameter());

        while (check("DELIMITER", ",")) {
            consume("DELIMITER", ",", "запятая ,");
            params->add(parseParameter());
        }

        return params;
    }

    ASTNode* parseParameter() {
        ASTNode* param = new ASTNode("parameter", "", current().line);

        Token type = consumeType();
        param->add(new ASTNode("type", type.value, type.line));

        Token name = consume("IDENTIFIER", "", "имя параметра");
        param->add(new ASTNode("name", name.value, name.line));

        return param;
    }

    ASTNode* parseBlock(const string& blockName) {
        ASTNode* block = new ASTNode(blockName, "", current().line);

        consume("DELIMITER", "{", "открывающая фигурная скобка {");

        while (!check("DELIMITER", "}") && !check("EOF")) {
            block->add(parseStatement());
        }

        consume("DELIMITER", "}", "закрывающая фигурная скобка }");
        return block;
    }

    ASTNode* parseStatement() {
        if (isType()) {
            return parseVarDecl();
        }

        if (check("KEYWORD", "if")) {
            return parseIf();
        }

        if (check("KEYWORD", "for")) {
            return parseFor();
        }

        if (check("KEYWORD", "while")) {
            return parseWhile();
        }

        if (check("KEYWORD", "return")) {
            return parseReturn();
        }

        if (check("IDENTIFIER", "cout")) {
            return parseCout();
        }

        if (check("IDENTIFIER")) {
            return parseAssign(true, "assign_stmt");
        }

        printError("оператор");

        if (!check("EOF")) {
            pos++;
        }

        return new ASTNode("error_stmt", "", current().line);
    }

    ASTNode* parseVarDecl() {
        ASTNode* node = new ASTNode("var_decl", "", current().line);

        Token type = consumeType();
        node->add(new ASTNode("type", type.value, type.line));

        Token name = consume("IDENTIFIER", "", "имя переменной");
        node->add(new ASTNode("name", name.value, name.line));

        if (check("OPERATOR", "=")) {
            consume("OPERATOR", "=", "оператор =");

            ASTNode* value = parseExpressionUntil({ ";" });
            node->add(new ASTNode("value", value->value, value->line));
            delete value;
        }

        consume("DELIMITER", ";", "разделитель ;");
        return node;
    }

    ASTNode* parseAssign(bool needSemicolon, const string& nodeName) {
        ASTNode* node = new ASTNode(nodeName, "", current().line);

        Token left = consume("IDENTIFIER", "", "идентификатор");
        node->add(new ASTNode("left", left.value, left.line));

        consume("OPERATOR", "=", "оператор =");

        vector<string> stops;
        stops.push_back(";");

        if (!needSemicolon) {
            stops.push_back(")");
        }

        ASTNode* right = parseExpressionUntil(stops);
        node->add(new ASTNode("right", right->value, right->line));
        delete right;

        if (needSemicolon) {
            consume("DELIMITER", ";", "разделитель ;");
        }

        return node;
    }

    ASTNode* parseIf() {
        ASTNode* node = new ASTNode("if_stmt", "", current().line);

        consume("KEYWORD", "if", "ключевое слово if");
        consume("DELIMITER", "(", "открывающая скобка (");

        ASTNode* condition = parseExpressionUntil({ ")" });
        node->add(new ASTNode("condition", condition->value, condition->line));
        delete condition;

        consume("DELIMITER", ")", "закрывающая скобка )");
        node->add(parseBlock("then_block"));

        if (check("KEYWORD", "else")) {
            consume("KEYWORD", "else", "ключевое слово else");
            node->add(parseBlock("else_block"));
        }

        return node;
    }

    ASTNode* parseFor() {
        ASTNode* node = new ASTNode("for_stmt", "", current().line);

        consume("KEYWORD", "for", "ключевое слово for");
        consume("DELIMITER", "(", "открывающая скобка (");

        node->add(parseAssign(false, "init"));
        consume("DELIMITER", ";", "разделитель ;");

        ASTNode* condition = parseExpressionUntil({ ";" });
        node->add(new ASTNode("condition", condition->value, condition->line));
        delete condition;

        consume("DELIMITER", ";", "разделитель ;");

        node->add(parseAssign(false, "update"));
        consume("DELIMITER", ")", "закрывающая скобка )");

        node->add(parseBlock("body"));
        return node;
    }

    ASTNode* parseWhile() {
        ASTNode* node = new ASTNode("while_stmt", "", current().line);

        consume("KEYWORD", "while", "ключевое слово while");
        consume("DELIMITER", "(", "открывающая скобка (");

        ASTNode* condition = parseExpressionUntil({ ")" });
        node->add(new ASTNode("condition", condition->value, condition->line));
        delete condition;

        consume("DELIMITER", ")", "закрывающая скобка )");
        node->add(parseBlock("body"));

        return node;
    }

    ASTNode* parseCout() {
        ASTNode* node = new ASTNode("cout_stmt", "", current().line);

        consume("IDENTIFIER", "cout", "идентификатор cout");

        while (check("OPERATOR", "<<")) {
            consume("OPERATOR", "<<", "оператор <<");

            ASTNode* item = parseExpressionUntil({ "<<", ";" });

            if (!item->value.empty()) {
                node->add(new ASTNode("output", item->value, item->line));
            }

            delete item;
        }

        consume("DELIMITER", ";", "разделитель ;");
        return node;
    }

    ASTNode* parseReturn() {
        ASTNode* node = new ASTNode("return_stmt", "", current().line);

        consume("KEYWORD", "return", "ключевое слово return");

        ASTNode* value = parseExpressionUntil({ ";" });
        node->add(new ASTNode("value", value->value, value->line));
        delete value;

        consume("DELIMITER", ";", "разделитель ;");
        return node;
    }

    ASTNode* parseExpressionUntil(const vector<string>& stopValues) {
        string expression;
        int expressionLine = current().line;
        int brackets = 0;

        while (!check("EOF")) {
            Token token = current();
            bool isStop = false;

            if (brackets == 0) {
                for (const string& stop : stopValues) {
                    if (token.value == stop) {
                        isStop = true;
                    }
                }
            }

            if (isStop) {
                break;
            }

            if (token.value == "(") {
                brackets++;
            }

            if (token.value == ")") {
                brackets--;
            }

            if (!expression.empty()) {
                expression += " ";
            }

            expression += token.value;
            pos++;
        }

        if (expression.empty()) {
            printError("выражение");
        }

        return new ASTNode("expression", expression, expressionLine);
    }
};

struct SymbolEntry {
    string name;
    string type;
    string scope;
    string kind;
    bool declared;
    bool initialized;
    int line;
};

struct FunctionInfo {
    string name;
    string returnType;
    vector<pair<string, string>> params;
    int line;
};

struct Triad {
    int number;
    string operation;
    string operand1;
    string operand2;
};

struct ExprToken {
    string type;
    string value;
};

struct ExprResult {
    string type;
    string place;
    bool ok;
};

class SemanticAnalyzer {
private:
    vector<SymbolEntry> symbols;
    vector<Triad> triads;
    vector<string> errors;
    map<string, FunctionInfo> functions;
    map<string, map<string, int>> scopeSymbols;
    vector<string> scopeStack;
    string currentFunction;
    string currentReturnType;
    int labelCounter;
    int blockCounter;

public:
    SemanticAnalyzer() {
        labelCounter = 1;
        blockCounter = 1;
    }

    void analyze(ASTNode* program) {
        collectFunctions(program);

        for (ASTNode* child : program->children) {
            if (child->name == "function_decl") {
                analyzeFunction(child);
            }
        }
    }

    bool success() const {
        return errors.empty();
    }

    const vector<SymbolEntry>& getSymbols() const {
        return symbols;
    }

    const vector<Triad>& getTriads() const {
        return triads;
    }

    const vector<string>& getErrors() const {
        return errors;
    }

private:
    ASTNode* findChild(ASTNode* node, const string& name) {
        for (ASTNode* child : node->children) {
            if (child->name == name) {
                return child;
            }
        }

        return nullptr;
    }

    vector<ASTNode*> findChildren(ASTNode* node, const string& name) {
        vector<ASTNode*> result;

        for (ASTNode* child : node->children) {
            if (child->name == name) {
                result.push_back(child);
            }
        }

        return result;
    }

    string currentScope() const {
        if (scopeStack.empty()) {
            return "global";
        }

        return scopeStack.back();
    }

    string newLabel() {
        string label = "L" + to_string(labelCounter);
        labelCounter++;
        return label;
    }

    string newBlockScope(const string& prefix) {
        string scope = currentFunction + "::" + prefix + to_string(blockCounter);
        blockCounter++;
        return scope;
    }

    int addTriad(const string& operation, const string& operand1, const string& operand2) {
        int number = static_cast<int>(triads.size()) + 1;
        triads.push_back({ number, operation, operand1, operand2 });
        return number;
    }

    void addError(int line, const string& message) {
        errors.push_back("Строка " + to_string(line) + ": " + message);
    }

    void collectFunctions(ASTNode* program) {
        for (ASTNode* child : program->children) {
            if (child->name != "function_decl") {
                continue;
            }

            ASTNode* returnType = findChild(child, "return_type");
            ASTNode* name = findChild(child, "name");
            ASTNode* parameters = findChild(child, "parameters");

            if (returnType == nullptr || name == nullptr) {
                continue;
            }

            if (functions.find(name->value) != functions.end()) {
                addError(name->line, "повторное объявление функции '" + name->value + "'");
                continue;
            }

            FunctionInfo info;
            info.name = name->value;
            info.returnType = returnType->value;
            info.line = child->line;

            if (parameters != nullptr) {
                for (ASTNode* param : parameters->children) {
                    ASTNode* paramType = findChild(param, "type");
                    ASTNode* paramName = findChild(param, "name");

                    if (paramType != nullptr && paramName != nullptr) {
                        info.params.push_back({ paramName->value, paramType->value });
                    }
                }
            }

            functions[info.name] = info;
        }
    }

    int declareVariable(const string& name, const string& type, const string& kind, int line, bool initialized) {
        string scope = currentScope();

        if (scopeSymbols[scope].find(name) != scopeSymbols[scope].end()) {
            addError(line, "повторное объявление переменной '" + name + "' в области видимости '" + scope + "'");
            return -1;
        }

        int index = static_cast<int>(symbols.size());
        symbols.push_back({ name, type, scope, kind, true, initialized, line });
        scopeSymbols[scope][name] = index;
        return index;
    }

    int lookupVariable(const string& name) {
        for (int i = static_cast<int>(scopeStack.size()) - 1; i >= 0; i--) {
            const string& scope = scopeStack[i];

            if (scopeSymbols[scope].find(name) != scopeSymbols[scope].end()) {
                return scopeSymbols[scope][name];
            }
        }

        return -1;
    }

    void markInitialized(const string& name) {
        int index = lookupVariable(name);

        if (index >= 0) {
            symbols[index].initialized = true;
        }
    }

    bool isTypeCompatible(const string& expected, const string& actual) {
        return expected == actual;
    }

    void analyzeFunction(ASTNode* functionNode) {
        ASTNode* nameNode = findChild(functionNode, "name");
        ASTNode* returnTypeNode = findChild(functionNode, "return_type");
        ASTNode* parameters = findChild(functionNode, "parameters");
        ASTNode* body = findChild(functionNode, "body");

        if (nameNode == nullptr || returnTypeNode == nullptr || body == nullptr) {
            return;
        }

        currentFunction = nameNode->value;
        currentReturnType = returnTypeNode->value;
        scopeStack.push_back(currentFunction);

        if (parameters != nullptr) {
            for (ASTNode* param : parameters->children) {
                ASTNode* type = findChild(param, "type");
                ASTNode* name = findChild(param, "name");

                if (type != nullptr && name != nullptr) {
                    declareVariable(name->value, type->value, "parameter", name->line, true);
                }
            }
        }

        analyzeBlock(body, false, currentFunction);
        scopeStack.pop_back();
    }

    void analyzeBlock(ASTNode* blockNode, bool createScope, const string& scopeName) {
        if (createScope) {
            scopeStack.push_back(scopeName);
        }

        for (ASTNode* child : blockNode->children) {
            analyzeStatement(child);
        }

        if (createScope) {
            scopeStack.pop_back();
        }
    }

    void analyzeStatement(ASTNode* node) {
        if (node->name == "var_decl") {
            analyzeVarDecl(node);
        }
        else if (node->name == "assign_stmt" || node->name == "init" || node->name == "update") {
            analyzeAssign(node);
        }
        else if (node->name == "return_stmt") {
            analyzeReturn(node);
        }
        else if (node->name == "if_stmt") {
            analyzeIf(node);
        }
        else if (node->name == "for_stmt") {
            analyzeFor(node);
        }
        else if (node->name == "while_stmt") {
            analyzeWhile(node);
        }
        else if (node->name == "cout_stmt") {
            analyzeCout(node);
        }
    }

    void analyzeVarDecl(ASTNode* node) {
        ASTNode* type = findChild(node, "type");
        ASTNode* name = findChild(node, "name");
        ASTNode* value = findChild(node, "value");

        if (type == nullptr || name == nullptr) {
            return;
        }

        int index = declareVariable(name->value, type->value, "variable", name->line, false);

        if (value != nullptr) {
            ExprResult result = analyzeExpression(value->value, value->line);

            if (result.ok && !isTypeCompatible(type->value, result.type)) {
                addError(value->line, "несоответствие типов при инициализации '" + name->value +
                    "': ожидается " + type->value + ", получено " + result.type);
            }

            if (index >= 0 && result.ok && isTypeCompatible(type->value, result.type)) {
                symbols[index].initialized = true;
            }

            if (!result.place.empty()) {
                addTriad(":=", name->value, result.place);
            }
        }
    }

    void analyzeAssign(ASTNode* node) {
        ASTNode* left = findChild(node, "left");
        ASTNode* right = findChild(node, "right");

        if (left == nullptr || right == nullptr) {
            return;
        }

        int index = lookupVariable(left->value);

        if (index < 0) {
            addError(left->line, "использование необъявленной переменной '" + left->value + "'");
        }

        ExprResult result = analyzeExpression(right->value, right->line);

        if (index >= 0 && result.ok && !isTypeCompatible(symbols[index].type, result.type)) {
            addError(right->line, "несоответствие типов в присваивании '" + left->value +
                "': ожидается " + symbols[index].type + ", получено " + result.type);
        }

        if (index >= 0 && result.ok && isTypeCompatible(symbols[index].type, result.type)) {
            markInitialized(left->value);
        }

        if (!result.place.empty()) {
            addTriad(":=", left->value, result.place);
        }
    }

    void analyzeReturn(ASTNode* node) {
        ASTNode* value = findChild(node, "value");

        if (value == nullptr) {
            return;
        }

        ExprResult result = analyzeExpression(value->value, value->line);

        if (result.ok && !isTypeCompatible(currentReturnType, result.type)) {
            addError(value->line, "тип возвращаемого значения функции '" + currentFunction +
                "' должен быть " + currentReturnType + ", получено " + result.type);
        }

        addTriad("return", result.place, "-");
    }

    void analyzeIf(ASTNode* node) {
        ASTNode* condition = findChild(node, "condition");
        ASTNode* thenBlock = findChild(node, "then_block");
        ASTNode* elseBlock = findChild(node, "else_block");

        ExprResult result = analyzeExpression(condition->value, condition->line);

        if (result.ok && result.type != "bool") {
            addError(condition->line, "условие оператора if должно иметь тип bool");
        }

        string elseLabel = newLabel();
        string endLabel = newLabel();
        addTriad("if_false", result.place, elseLabel);

        if (thenBlock != nullptr) {
            analyzeBlock(thenBlock, true, newBlockScope("if"));
        }

        addTriad("goto", endLabel, "-");
        addTriad("label", elseLabel, "-");

        if (elseBlock != nullptr) {
            analyzeBlock(elseBlock, true, newBlockScope("else"));
        }

        addTriad("label", endLabel, "-");
    }

    void analyzeFor(ASTNode* node) {
        ASTNode* init = findChild(node, "init");
        ASTNode* condition = findChild(node, "condition");
        ASTNode* update = findChild(node, "update");
        ASTNode* body = findChild(node, "body");

        if (init != nullptr) {
            analyzeAssign(init);
        }

        string startLabel = newLabel();
        string endLabel = newLabel();
        addTriad("label", startLabel, "-");

        if (condition != nullptr) {
            ExprResult conditionResult = analyzeExpression(condition->value, condition->line);

            if (conditionResult.ok && conditionResult.type != "bool") {
                addError(condition->line, "условие оператора for должно иметь тип bool");
            }

            addTriad("if_false", conditionResult.place, endLabel);
        }

        if (body != nullptr) {
            analyzeBlock(body, true, newBlockScope("for"));
        }

        if (update != nullptr) {
            analyzeAssign(update);
        }

        addTriad("goto", startLabel, "-");
        addTriad("label", endLabel, "-");
    }

    void analyzeWhile(ASTNode* node) {
        ASTNode* condition = findChild(node, "condition");
        ASTNode* body = findChild(node, "body");

        string startLabel = newLabel();
        string endLabel = newLabel();
        addTriad("label", startLabel, "-");

        if (condition != nullptr) {
            ExprResult conditionResult = analyzeExpression(condition->value, condition->line);

            if (conditionResult.ok && conditionResult.type != "bool") {
                addError(condition->line, "условие оператора while должно иметь тип bool");
            }

            addTriad("if_false", conditionResult.place, endLabel);
        }

        if (body != nullptr) {
            analyzeBlock(body, true, newBlockScope("while"));
        }

        addTriad("goto", startLabel, "-");
        addTriad("label", endLabel, "-");
    }

    void analyzeCout(ASTNode* node) {
        for (ASTNode* output : node->children) {
            ExprResult result = analyzeExpression(output->value, output->line);

            if (result.ok && result.type != "int" && result.type != "bool" &&
                result.type != "string" && result.type != "manipulator") {
                addError(output->line, "оператор cout не поддерживает тип " + result.type);
            }

            addTriad("out", result.place, "-");
        }
    }

    vector<ExprToken> tokenizeExpression(const string& expression) {
        vector<ExprToken> result;
        int i = 0;

        while (i < static_cast<int>(expression.size())) {
            char c = expression[i];

            if (isspace(static_cast<unsigned char>(c))) {
                i++;
            }
            else if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
                string word;

                while (i < static_cast<int>(expression.size()) &&
                    (isalnum(static_cast<unsigned char>(expression[i])) || expression[i] == '_')) {
                    word += expression[i];
                    i++;
                }

                if (isBoolConst(word)) {
                    result.push_back({ "BOOL", word });
                }
                else {
                    result.push_back({ "IDENT", word });
                }
            }
            else if (isdigit(static_cast<unsigned char>(c))) {
                string number;

                while (i < static_cast<int>(expression.size()) && isdigit(static_cast<unsigned char>(expression[i]))) {
                    number += expression[i];
                    i++;
                }

                result.push_back({ "NUMBER", number });
            }
            else if (c == '"') {
                string text;
                text += c;
                i++;

                while (i < static_cast<int>(expression.size())) {
                    text += expression[i];

                    if (expression[i] == '"') {
                        i++;
                        break;
                    }

                    i++;
                }

                result.push_back({ "STRING", text });
            }
            else {
                string two;

                if (i + 1 < static_cast<int>(expression.size())) {
                    two += expression[i];
                    two += expression[i + 1];
                }

                if (isTwoCharOperator(two)) {
                    result.push_back({ "OP", two });
                    i += 2;
                }
                else if (c == '(') {
                    result.push_back({ "LPAREN", "(" });
                    i++;
                }
                else if (c == ')') {
                    result.push_back({ "RPAREN", ")" });
                    i++;
                }
                else if (c == ',') {
                    result.push_back({ "COMMA", "," });
                    i++;
                }
                else if (isOneCharOperator(c)) {
                    string op;
                    op += c;
                    result.push_back({ "OP", op });
                    i++;
                }
                else {
                    string unknown;
                    unknown += c;
                    result.push_back({ "UNKNOWN", unknown });
                    i++;
                }
            }
        }

        result.push_back({ "END", "" });
        return result;
    }

    ExprResult analyzeExpression(const string& expression, int line) {
        vector<ExprToken> expressionTokens = tokenizeExpression(expression);

        class ExpressionParser {
        private:
            SemanticAnalyzer& analyzer;
            vector<ExprToken> tokens;
            int pos;
            int line;

        public:
            ExpressionParser(SemanticAnalyzer& owner, const vector<ExprToken>& inputTokens, int expressionLine)
                : analyzer(owner), tokens(inputTokens), pos(0), line(expressionLine) {
            }

            ExprResult parse() {
                ExprResult result = parseOr();

                if (!check("END")) {
                    analyzer.addError(line, "лишний токен в выражении '" + current().value + "'");
                    result.ok = false;
                }

                return result;
            }

        private:
            ExprToken current() const {
                if (pos < static_cast<int>(tokens.size())) {
                    return tokens[pos];
                }

                return { "END", "" };
            }

            bool check(const string& type, const string& value = "") const {
                ExprToken token = current();

                if (token.type != type) {
                    return false;
                }

                if (!value.empty() && token.value != value) {
                    return false;
                }

                return true;
            }

            ExprToken consume() {
                ExprToken token = current();

                if (!check("END")) {
                    pos++;
                }

                return token;
            }

            ExprResult parseOr() {
                ExprResult left = parseAnd();

                while (check("OP", "||")) {
                    string op = consume().value;
                    ExprResult right = parseAnd();
                    left = binary(left, op, right);
                }

                return left;
            }

            ExprResult parseAnd() {
                ExprResult left = parseEquality();

                while (check("OP", "&&")) {
                    string op = consume().value;
                    ExprResult right = parseEquality();
                    left = binary(left, op, right);
                }

                return left;
            }

            ExprResult parseEquality() {
                ExprResult left = parseRelational();

                while (check("OP", "==") || check("OP", "!=")) {
                    string op = consume().value;
                    ExprResult right = parseRelational();
                    left = binary(left, op, right);
                }

                return left;
            }

            ExprResult parseRelational() {
                ExprResult left = parseAdditive();

                while (check("OP", "<") || check("OP", ">") || check("OP", "<=") || check("OP", ">=")) {
                    string op = consume().value;
                    ExprResult right = parseAdditive();
                    left = binary(left, op, right);
                }

                return left;
            }

            ExprResult parseAdditive() {
                ExprResult left = parseMultiplicative();

                while (check("OP", "+") || check("OP", "-")) {
                    string op = consume().value;
                    ExprResult right = parseMultiplicative();
                    left = binary(left, op, right);
                }

                return left;
            }

            ExprResult parseMultiplicative() {
                ExprResult left = parseUnary();

                while (check("OP", "*") || check("OP", "/")) {
                    string op = consume().value;
                    ExprResult right = parseUnary();
                    left = binary(left, op, right);
                }

                return left;
            }

            ExprResult parseUnary() {
                if (check("OP", "!")) {
                    consume();
                    ExprResult value = parseUnary();

                    if (value.ok && value.type != "bool") {
                        analyzer.addError(line, "операция ! применима только к bool");
                        value.ok = false;
                    }

                    int number = analyzer.addTriad("!", value.place, "-");
                    return { "bool", "^" + to_string(number), value.ok };
                }

                if (check("OP", "-")) {
                    consume();
                    ExprResult value = parseUnary();

                    if (value.ok && value.type != "int") {
                        analyzer.addError(line, "унарный минус применим только к int");
                        value.ok = false;
                    }

                    int number = analyzer.addTriad("uminus", value.place, "-");
                    return { "int", "^" + to_string(number), value.ok };
                }

                return parsePrimary();
            }

            ExprResult parsePrimary() {
                if (check("NUMBER")) {
                    string value = consume().value;
                    return { "int", value, true };
                }

                if (check("BOOL")) {
                    string value = consume().value;
                    return { "bool", value, true };
                }

                if (check("STRING")) {
                    string value = consume().value;
                    return { "string", value, true };
                }

                if (check("IDENT")) {
                    string name = consume().value;

                    if (check("LPAREN")) {
                        return parseFunctionCall(name);
                    }

                    if (name == "endl") {
                        return { "manipulator", "endl", true };
                    }

                    int index = analyzer.lookupVariable(name);

                    if (index < 0) {
                        analyzer.addError(line, "использование необъявленной переменной '" + name + "'");
                        return { "unknown", name, false };
                    }

                    if (!analyzer.symbols[index].initialized) {
                        analyzer.addError(line, "использование неинициализированной переменной '" + name + "'");
                        return { analyzer.symbols[index].type, name, false };
                    }

                    return { analyzer.symbols[index].type, name, true };
                }

                if (check("LPAREN")) {
                    consume();
                    ExprResult result = parseOr();

                    if (check("RPAREN")) {
                        consume();
                    }
                    else {
                        analyzer.addError(line, "ожидалась закрывающая скобка в выражении");
                        result.ok = false;
                    }

                    return result;
                }

                analyzer.addError(line, "недопустимое выражение около '" + current().value + "'");
                consume();
                return { "unknown", "", false };
            }

            ExprResult parseFunctionCall(const string& name) {
                consume();
                vector<ExprResult> args;

                if (!check("RPAREN")) {
                    args.push_back(parseOr());

                    while (check("COMMA")) {
                        consume();
                        args.push_back(parseOr());
                    }
                }

                if (check("RPAREN")) {
                    consume();
                }
                else {
                    analyzer.addError(line, "ожидалась закрывающая скобка вызова функции '" + name + "'");
                }

                if (analyzer.functions.find(name) == analyzer.functions.end()) {
                    analyzer.addError(line, "вызов необъявленной функции '" + name + "'");
                    return { "unknown", name, false };
                }

                FunctionInfo info = analyzer.functions[name];
                bool ok = true;

                if (info.params.size() != args.size()) {
                    analyzer.addError(line, "функция '" + name + "' ожидает аргументов: " +
                        to_string(info.params.size()) + ", передано: " + to_string(args.size()));
                    ok = false;
                }

                int checkedCount = min(static_cast<int>(info.params.size()), static_cast<int>(args.size()));

                for (int i = 0; i < checkedCount; i++) {
                    if (args[i].ok && info.params[i].second != args[i].type) {
                        analyzer.addError(line, "тип аргумента " + to_string(i + 1) + " функции '" + name +
                            "' должен быть " + info.params[i].second + ", получено " + args[i].type);
                        ok = false;
                    }

                    ok = ok && args[i].ok;
                }

                for (const ExprResult& arg : args) {
                    analyzer.addTriad("param", arg.place, "-");
                }

                int number = analyzer.addTriad("call", name, to_string(args.size()));
                return { info.returnType, "^" + to_string(number), ok };
            }

            ExprResult binary(const ExprResult& left, const string& op, const ExprResult& right) {
                bool ok = left.ok && right.ok;
                string resultType = "unknown";

                if (op == "+" || op == "-" || op == "*" || op == "/") {
                    resultType = "int";

                    if (left.ok && right.ok && (left.type != "int" || right.type != "int")) {
                        analyzer.addError(line, "операция " + op + " требует операнды типа int");
                        ok = false;
                    }
                }
                else if (op == "<" || op == ">" || op == "<=" || op == ">=") {
                    resultType = "bool";

                    if (left.ok && right.ok && (left.type != "int" || right.type != "int")) {
                        analyzer.addError(line, "операция " + op + " требует операнды типа int");
                        ok = false;
                    }
                }
                else if (op == "==" || op == "!=") {
                    resultType = "bool";

                    if (left.ok && right.ok && left.type != right.type) {
                        analyzer.addError(line, "операция " + op + " требует совпадающие типы операндов");
                        ok = false;
                    }
                }
                else if (op == "&&" || op == "||") {
                    resultType = "bool";

                    if (left.ok && right.ok && (left.type != "bool" || right.type != "bool")) {
                        analyzer.addError(line, "логическая операция " + op + " требует операнды типа bool");
                        ok = false;
                    }
                }

                int number = analyzer.addTriad(op, left.place, right.place);
                return { resultType, "^" + to_string(number), ok };
            }
        };

        ExpressionParser parser(*this, expressionTokens, line);
        return parser.parse();
    }
};

void printTokens(const vector<Token>& tokens) {
    cout << "[";
    bool first = true;

    for (const Token& token : tokens) {
        if (token.type == "EOF") {
            continue;
        }

        if (!first) {
            cout << ", ";
        }

        cout << "(" << token.type << ", " << token.value << ")";
        first = false;
    }

    cout << "]" << endl;
}

void printAST(ASTNode* node, string prefix = "", bool isLast = true, bool isRoot = true) {
    if (isRoot) {
        cout << node->name;

        if (!node->value.empty()) {
            cout << ": " << node->value;
        }

        cout << endl;
    }
    else {
        cout << prefix << (isLast ? "└── " : "├── ") << node->name;

        if (!node->value.empty()) {
            cout << ": " << node->value;
        }

        cout << endl;
    }

    for (int i = 0; i < static_cast<int>(node->children.size()); i++) {
        bool lastChild = i == static_cast<int>(node->children.size()) - 1;
        string newPrefix = prefix;

        if (!isRoot) {
            newPrefix += isLast ? "    " : "│   ";
        }

        printAST(node->children[i], newPrefix, lastChild, false);
    }
}

void printSymbolTable(const vector<SymbolEntry>& symbols) {
    cout << left
        << setw(12) << "Name"
        << setw(10) << "Type"
        << setw(18) << "Scope"
        << setw(12) << "Kind"
        << setw(11) << "Declared"
        << setw(13) << "Initialized"
        << "Line" << endl;

    cout << string(82, '-') << endl;

    for (const SymbolEntry& symbol : symbols) {
        cout << left
            << setw(12) << symbol.name
            << setw(10) << symbol.type
            << setw(18) << symbol.scope
            << setw(12) << symbol.kind
            << setw(11) << (symbol.declared ? "true" : "false")
            << setw(13) << (symbol.initialized ? "true" : "false")
            << symbol.line << endl;
    }
}

void printTriads(const vector<Triad>& triads) {
    for (const Triad& triad : triads) {
        cout << triad.number << ") (" << triad.operation << ", "
            << triad.operand1 << ", " << triad.operand2 << ")" << endl;
    }
}

string readFile(const string& fileName) {
    ifstream file(fileName);

    if (!file.is_open()) {
        return "";
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const string& fileName, const string& content) {
    ofstream file(fileName);

    if (file.is_open()) {
        file << content;
    }
}

void printSection(const string& title) {
    cout << endl << "==================== " << title << " ====================" << endl << endl;
}

void configureConsoleLocale() {
    setlocale(LC_ALL, "Russian");

#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    if (setlocale(LC_ALL, ".UTF-8") == nullptr) {
        setlocale(LC_ALL, "Russian");
    }
#endif
}

int main() {
    configureConsoleLocale();

    string sourceCode = readFile("test.cpp");

    if (sourceCode.empty()) {
        cout << "Ошибка: не удалось открыть файл test.cpp" << endl;
        return 1;
    }

    printSection("ЛР1. Препроцессинг");
    vector<string> preprocessErrors;
    PreprocessResult preprocessed = preprocess(sourceCode);

    if (!preprocessed.ok) {
        for (const string& error : preprocessed.errors) {
            cout << "Ошибка: " << error << endl;
        }

        return 1;
    }

    writeFile("result.cpp", preprocessed.code);
    cout << preprocessed.code << endl;
    cout << "Препроцессинг завершён успешно. Очищенный код записан в result.cpp" << endl;

    printSection("ЛР2. Лексический анализ");
    vector<string> lexicalErrors;
    vector<Token> tokens = lexicalAnalyze(preprocessed.code, lexicalErrors);
    printTokens(tokens);

    if (!lexicalErrors.empty()) {
        cout << endl << "Лексический анализ завершён с ошибками:" << endl;

        for (const string& error : lexicalErrors) {
            cout << error << endl;
        }

        return 1;
    }

    cout << endl << "Лексический анализ завершён успешно. Обнаружено токенов: " << tokens.size() - 1 << endl;

    printSection("ЛР3. Синтаксический анализ");
    Parser parser(tokens);
    ASTNode* tree = parser.parseProgram();
    printAST(tree);

    if (!parser.success()) {
        cout << endl << "Синтаксический анализ завершён с ошибками." << endl;
        delete tree;
        return 1;
    }

    cout << endl << "Синтаксический анализ завершён успешно. Ошибок не найдено." << endl;

    printSection("ЛР4. Семантический анализ");
    SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.analyze(tree);

    cout << "Таблица символов" << endl;
    printSymbolTable(semanticAnalyzer.getSymbols());

    cout << endl << "Сообщения семантического анализатора" << endl;

    if (semanticAnalyzer.success()) {
        cout << "Семантический анализ завершён успешно. Ошибок не найдено." << endl;
    }
    else {
        for (const string& error : semanticAnalyzer.getErrors()) {
            cout << error << endl;
        }
    }

    cout << endl << "Промежуточное представление: триады" << endl;
    printTriads(semanticAnalyzer.getTriads());

    delete tree;
    return semanticAnalyzer.success() ? 0 : 1;
}
