#include <iostream>
#include <vector>
#include <string>

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
    vector<ASTNode*> children;

    ASTNode(string nodeName, string nodeValue = "") {
        name = nodeName;
        value = nodeValue;
    }

    void add(ASTNode* child) {
        children.push_back(child);
    }
};

class Parser {
private:
    vector<Token> tokens;
    int pos;
    bool hasError;

public:
    Parser(vector<Token> inputTokens) {
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

    bool success() {
        return !hasError;
    }

private:
    Token current() {
        if (pos < tokens.size()) {
            return tokens[pos];
        }

        return { "EOF", "EOF", -1, -1 };
    }

    bool check(string type, string value = "") {
        Token token = current();

        if (token.type != type) {
            return false;
        }

        if (value != "" && token.value != value) {
            return false;
        }

        return true;
    }

    bool isType() {
        return check("KEYWORD", "int") || check("KEYWORD", "bool");
    }

    Token consume(string type, string value, string expected) {
        Token token = current();

        if (check(type, value)) {
            pos++;
            return token;
        }

        printError(expected);
        pos++;

        return token;
    }

    Token consumeType() {
        Token token = current();

        if (isType()) {
            pos++;
            return token;
        }

        printError("тип данных int или bool");
        pos++;

        return token;
    }

    void printError(string expected) {
        Token token = current();

        cout << "Синтаксическая ошибка: ожидалось " << expected
            << ", найдено " << token.type << " '" << token.value << "'"
            << " в позиции " << token.line << ":" << token.column << endl;

        hasError = true;
    }

    ASTNode* parseInclude() {
        ASTNode* node = new ASTNode("include_directive");

        consume("PREPROCESSOR", "#include", "директива #include");
        consume("OPERATOR", "<", "символ <");

        Token library = consume("IDENTIFIER", "", "имя библиотеки");
        node->add(new ASTNode("library", library.value));

        consume("OPERATOR", ">", "символ >");

        return node;
    }

    ASTNode* parseUsing() {
        ASTNode* node = new ASTNode("using_directive");

        consume("KEYWORD", "using", "ключевое слово using");
        consume("KEYWORD", "namespace", "ключевое слово namespace");

        Token namespaceName = consume("IDENTIFIER", "", "имя пространства имён");
        node->add(new ASTNode("namespace", namespaceName.value));

        consume("DELIMITER", ";", "разделитель ;");

        return node;
    }

    ASTNode* parseFunction() {
        ASTNode* node = new ASTNode("function_decl");

        Token returnType = consumeType();
        node->add(new ASTNode("return_type", returnType.value));

        Token functionName = consume("IDENTIFIER", "", "имя функции");
        node->add(new ASTNode("name", functionName.value));

        consume("DELIMITER", "(", "открывающая скобка (");

        node->add(parseParameters());

        consume("DELIMITER", ")", "закрывающая скобка )");

        node->add(parseBlock("body"));

        return node;
    }

    ASTNode* parseParameters() {
        ASTNode* params = new ASTNode("parameters");

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
        ASTNode* param = new ASTNode("parameter");

        Token type = consumeType();
        param->add(new ASTNode("type", type.value));

        Token name = consume("IDENTIFIER", "", "имя параметра");
        param->add(new ASTNode("name", name.value));

        return param;
    }

    ASTNode* parseBlock(string blockName) {
        ASTNode* block = new ASTNode(blockName);

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
        pos++;

        return new ASTNode("error_stmt");
    }

    ASTNode* parseVarDecl() {
        ASTNode* node = new ASTNode("var_decl");

        Token type = consumeType();
        node->add(new ASTNode("type", type.value));

        Token name = consume("IDENTIFIER", "", "имя переменной");
        node->add(new ASTNode("name", name.value));

        if (check("OPERATOR", "=")) {
            consume("OPERATOR", "=", "оператор =");

            ASTNode* value = parseExpressionUntil({ ";" });
            node->add(new ASTNode("value", value->value));
        }

        consume("DELIMITER", ";", "разделитель ;");

        return node;
    }

    ASTNode* parseAssign(bool needSemicolon, string nodeName) {
        ASTNode* node = new ASTNode(nodeName);

        Token left = consume("IDENTIFIER", "", "идентификатор");
        node->add(new ASTNode("left", left.value));

        consume("OPERATOR", "=", "оператор =");

        vector<string> stops;

        if (needSemicolon) {
            stops.push_back(";");
        }
        else {
            stops.push_back(";");
            stops.push_back(")");
        }

        ASTNode* right = parseExpressionUntil(stops);
        node->add(new ASTNode("right", right->value));

        if (needSemicolon) {
            consume("DELIMITER", ";", "разделитель ;");
        }

        return node;
    }

    ASTNode* parseIf() {
        ASTNode* node = new ASTNode("if_stmt");

        consume("KEYWORD", "if", "ключевое слово if");
        consume("DELIMITER", "(", "открывающая скобка (");

        ASTNode* condition = parseExpressionUntil({ ")" });
        node->add(new ASTNode("condition", condition->value));

        consume("DELIMITER", ")", "закрывающая скобка )");

        node->add(parseBlock("then_block"));

        if (check("KEYWORD", "else")) {
            consume("KEYWORD", "else", "ключевое слово else");
            node->add(parseBlock("else_block"));
        }

        return node;
    }

    ASTNode* parseFor() {
        ASTNode* node = new ASTNode("for_stmt");

        consume("KEYWORD", "for", "ключевое слово for");
        consume("DELIMITER", "(", "открывающая скобка (");

        ASTNode* init = parseAssign(false, "init");
        node->add(init);

        consume("DELIMITER", ";", "разделитель ;");

        ASTNode* condition = parseExpressionUntil({ ";" });
        node->add(new ASTNode("condition", condition->value));

        consume("DELIMITER", ";", "разделитель ;");

        ASTNode* update = parseAssign(false, "update");
        node->add(update);

        consume("DELIMITER", ")", "закрывающая скобка )");

        node->add(parseBlock("body"));

        return node;
    }

    ASTNode* parseWhile() {
        ASTNode* node = new ASTNode("while_stmt");

        consume("KEYWORD", "while", "ключевое слово while");
        consume("DELIMITER", "(", "открывающая скобка (");

        ASTNode* condition = parseExpressionUntil({ ")" });
        node->add(new ASTNode("condition", condition->value));

        consume("DELIMITER", ")", "закрывающая скобка )");

        node->add(parseBlock("body"));

        return node;
    }

    ASTNode* parseCout() {
        ASTNode* node = new ASTNode("cout_stmt");

        consume("IDENTIFIER", "cout", "идентификатор cout");

        while (check("OPERATOR", "<<")) {
            consume("OPERATOR", "<<", "оператор <<");

            ASTNode* item = parseExpressionUntil({ "<<", ";" });

            if (item->value != "") {
                node->add(new ASTNode("output", item->value));
            }
        }

        consume("DELIMITER", ";", "разделитель ;");

        return node;
    }

    ASTNode* parseReturn() {
        ASTNode* node = new ASTNode("return_stmt");

        consume("KEYWORD", "return", "ключевое слово return");

        ASTNode* value = parseExpressionUntil({ ";" });
        node->add(new ASTNode("value", value->value));

        consume("DELIMITER", ";", "разделитель ;");

        return node;
    }

    ASTNode* parseExpressionUntil(vector<string> stopValues) {
        string expression = "";
        int brackets = 0;

        while (!check("EOF")) {
            Token token = current();

            bool isStop = false;

            if (brackets == 0) {
                for (string stop : stopValues) {
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

            if (expression != "") {
                expression += " ";
            }

            expression += token.value;
            pos++;
        }

        if (expression == "") {
            printError("выражение");
        }

        return new ASTNode("expression", expression);
    }
};

void printAST(ASTNode* node, string prefix = "", bool isLast = true, bool isRoot = true) {
    if (isRoot) {
        cout << node->name;

        if (node->value != "") {
            cout << ": " << node->value;
        }

        cout << endl;
    }
    else {
        cout << prefix;
        cout << (isLast ? "└── " : "├── ");
        cout << node->name;

        if (node->value != "") {
            cout << ": " << node->value;
        }

        cout << endl;
    }

    for (int i = 0; i < node->children.size(); i++) {
        bool lastChild = i == node->children.size() - 1;

        string newPrefix = prefix;

        if (!isRoot) {
            if (isLast) {
                newPrefix += "    ";
            }
            else {
                newPrefix += "│   ";
            }
        }

        printAST(node->children[i], newPrefix, lastChild, false);
    }
}

void printTokens(vector<Token> tokens) {
    cout << "[";

    bool first = true;

    for (int i = 0; i < tokens.size(); i++) {
        if (tokens[i].type == "EOF") {
            continue;
        }

        if (!first) {
            cout << ", ";
        }

        cout << "(" << tokens[i].type << ", " << tokens[i].value << ")";
        first = false;
    }

    cout << "]" << endl;
}

int main() {
	setlocale(LC_ALL, "Russian");
    vector<Token> tokens = {
        {"PREPROCESSOR", "#include", 1, 1},
        {"OPERATOR", "<", 1, 10},
        {"IDENTIFIER", "iostream", 1, 11},
        {"OPERATOR", ">", 1, 19},

        {"KEYWORD", "using", 2, 1},
        {"KEYWORD", "namespace", 2, 7},
        {"IDENTIFIER", "std", 2, 17},
        {"DELIMITER", ";", 2, 20},

        {"KEYWORD", "int", 4, 1},
        {"IDENTIFIER", "add", 4, 5},
        {"DELIMITER", "(", 4, 8},
        {"KEYWORD", "int", 4, 9},
        {"IDENTIFIER", "a", 4, 13},
        {"DELIMITER", ",", 4, 14},
        {"KEYWORD", "int", 4, 16},
        {"IDENTIFIER", "b", 4, 20},
        {"DELIMITER", ")", 4, 21},
        {"DELIMITER", "{", 4, 23},

        {"KEYWORD", "int", 5, 5},
        {"IDENTIFIER", "result", 5, 9},
        {"OPERATOR", "=", 5, 16},
        {"IDENTIFIER", "a", 5, 18},
        {"OPERATOR", "+", 5, 20},
        {"IDENTIFIER", "b", 5, 22},
        {"DELIMITER", ";", 5, 23},

        {"KEYWORD", "return", 6, 5},
        {"IDENTIFIER", "result", 6, 12},
        {"DELIMITER", ";", 6, 18},
        {"DELIMITER", "}", 7, 1},

        {"KEYWORD", "int", 9, 1},
        {"IDENTIFIER", "main", 9, 5},
        {"DELIMITER", "(", 9, 9},
        {"DELIMITER", ")", 9, 10},
        {"DELIMITER", "{", 9, 12},

        {"KEYWORD", "int", 13, 5},
        {"IDENTIFIER", "a", 13, 9},
        {"OPERATOR", "=", 13, 11},
        {"CONSTANT_INT", "5", 13, 13},
        {"DELIMITER", ";", 13, 14},

        {"KEYWORD", "int", 14, 5},
        {"IDENTIFIER", "b", 14, 9},
        {"OPERATOR", "=", 14, 11},
        {"CONSTANT_INT", "3", 14, 13},
        {"DELIMITER", ";", 14, 14},

        {"KEYWORD", "int", 15, 5},
        {"IDENTIFIER", "sum", 15, 9},
        {"OPERATOR", "=", 15, 13},
        {"CONSTANT_INT", "0", 15, 15},
        {"DELIMITER", ";", 15, 16},

        {"KEYWORD", "int", 16, 5},
        {"IDENTIFIER", "i", 16, 9},
        {"OPERATOR", "=", 16, 11},
        {"CONSTANT_INT", "0", 16, 13},
        {"DELIMITER", ";", 16, 14},

        {"KEYWORD", "bool", 17, 5},
        {"IDENTIFIER", "more", 17, 10},
        {"OPERATOR", "=", 17, 15},
        {"KEYWORD", "false", 17, 17},
        {"DELIMITER", ";", 17, 22},

        {"IDENTIFIER", "sum", 19, 5},
        {"OPERATOR", "=", 19, 9},
        {"IDENTIFIER", "add", 19, 11},
        {"DELIMITER", "(", 19, 14},
        {"IDENTIFIER", "a", 19, 15},
        {"DELIMITER", ",", 19, 16},
        {"IDENTIFIER", "b", 19, 18},
        {"DELIMITER", ")", 19, 19},
        {"DELIMITER", ";", 19, 20},

        {"IDENTIFIER", "more", 20, 5},
        {"OPERATOR", "=", 20, 10},
        {"IDENTIFIER", "sum", 20, 12},
        {"OPERATOR", ">", 20, 16},
        {"CONSTANT_INT", "5", 20, 18},
        {"OPERATOR", "&&", 20, 20},
        {"IDENTIFIER", "a", 20, 23},
        {"OPERATOR", "<", 20, 25},
        {"CONSTANT_INT", "10", 20, 27},
        {"DELIMITER", ";", 20, 29},

        {"KEYWORD", "if", 22, 5},
        {"DELIMITER", "(", 22, 8},
        {"IDENTIFIER", "more", 22, 9},
        {"DELIMITER", ")", 22, 13},
        {"DELIMITER", "{", 22, 15},

        {"IDENTIFIER", "cout", 23, 9},
        {"OPERATOR", "<<", 23, 14},
        {"STRING", "\"Sum is bigger than 5\"", 23, 17},
        {"OPERATOR", "<<", 23, 40},
        {"IDENTIFIER", "endl", 23, 43},
        {"DELIMITER", ";", 23, 47},

        {"DELIMITER", "}", 24, 5},
        {"KEYWORD", "else", 25, 5},
        {"DELIMITER", "{", 25, 10},

        {"IDENTIFIER", "cout", 26, 9},
        {"OPERATOR", "<<", 26, 14},
        {"STRING", "\"Sum is not bigger than 5\"", 26, 17},
        {"OPERATOR", "<<", 26, 44},
        {"IDENTIFIER", "endl", 26, 47},
        {"DELIMITER", ";", 26, 51},

        {"DELIMITER", "}", 27, 5},

        {"KEYWORD", "for", 29, 5},
        {"DELIMITER", "(", 29, 9},
        {"IDENTIFIER", "i", 29, 10},
        {"OPERATOR", "=", 29, 12},
        {"CONSTANT_INT", "0", 29, 14},
        {"DELIMITER", ";", 29, 15},
        {"IDENTIFIER", "i", 29, 17},
        {"OPERATOR", "<", 29, 19},
        {"CONSTANT_INT", "3", 29, 21},
        {"DELIMITER", ";", 29, 22},
        {"IDENTIFIER", "i", 29, 24},
        {"OPERATOR", "=", 29, 26},
        {"IDENTIFIER", "i", 29, 28},
        {"OPERATOR", "+", 29, 30},
        {"CONSTANT_INT", "1", 29, 32},
        {"DELIMITER", ")", 29, 33},
        {"DELIMITER", "{", 29, 35},

        {"IDENTIFIER", "cout", 30, 9},
        {"OPERATOR", "<<", 30, 14},
        {"STRING", "\"for: \"", 30, 17},
        {"OPERATOR", "<<", 30, 25},
        {"IDENTIFIER", "i", 30, 28},
        {"OPERATOR", "<<", 30, 30},
        {"IDENTIFIER", "endl", 30, 33},
        {"DELIMITER", ";", 30, 37},

        {"DELIMITER", "}", 31, 5},

        {"IDENTIFIER", "i", 33, 5},
        {"OPERATOR", "=", 33, 7},
        {"CONSTANT_INT", "0", 33, 9},
        {"DELIMITER", ";", 33, 10},

        {"KEYWORD", "while", 34, 5},
        {"DELIMITER", "(", 34, 11},
        {"IDENTIFIER", "i", 34, 12},
        {"OPERATOR", "<", 34, 14},
        {"CONSTANT_INT", "2", 34, 16},
        {"DELIMITER", ")", 34, 17},
        {"DELIMITER", "{", 34, 19},

        {"IDENTIFIER", "cout", 35, 9},
        {"OPERATOR", "<<", 35, 14},
        {"STRING", "\"while: \"", 35, 17},
        {"OPERATOR", "<<", 35, 27},
        {"IDENTIFIER", "i", 35, 30},
        {"OPERATOR", "<<", 35, 32},
        {"IDENTIFIER", "endl", 35, 35},
        {"DELIMITER", ";", 35, 39},

        {"IDENTIFIER", "i", 36, 9},
        {"OPERATOR", "=", 36, 11},
        {"IDENTIFIER", "i", 36, 13},
        {"OPERATOR", "+", 36, 15},
        {"CONSTANT_INT", "1", 36, 17},
        {"DELIMITER", ";", 36, 18},

        {"DELIMITER", "}", 37, 5},

        {"IDENTIFIER", "cout", 39, 5},
        {"OPERATOR", "<<", 39, 10},
        {"STRING", "\"a = \"", 39, 13},
        {"OPERATOR", "<<", 39, 20},
        {"IDENTIFIER", "a", 39, 23},
        {"OPERATOR", "<<", 39, 25},
        {"IDENTIFIER", "endl", 39, 28},
        {"DELIMITER", ";", 39, 32},

        {"IDENTIFIER", "cout", 40, 5},
        {"OPERATOR", "<<", 40, 10},
        {"STRING", "\"b = \"", 40, 13},
        {"OPERATOR", "<<", 40, 20},
        {"IDENTIFIER", "b", 40, 23},
        {"OPERATOR", "<<", 40, 25},
        {"IDENTIFIER", "endl", 40, 28},
        {"DELIMITER", ";", 40, 32},

        {"IDENTIFIER", "cout", 41, 5},
        {"OPERATOR", "<<", 41, 10},
        {"STRING", "\"sum = \"", 41, 13},
        {"OPERATOR", "<<", 41, 22},
        {"IDENTIFIER", "sum", 41, 25},
        {"OPERATOR", "<<", 41, 29},
        {"IDENTIFIER", "endl", 41, 32},
        {"DELIMITER", ";", 41, 36},

        {"KEYWORD", "return", 43, 5},
        {"CONSTANT_INT", "0", 43, 12},
        {"DELIMITER", ";", 43, 13},

        {"DELIMITER", "}", 44, 1},

        {"EOF", "EOF", 45, 1}
    };

    Parser parser(tokens);
    ASTNode* tree = parser.parseProgram();

    cout << "Пример результата работы модуля:" << endl << endl;

    cout << "Входные данные (поток токенов из ЛР2)" << endl << endl;
    printTokens(tokens);

    cout << endl;
    cout << "Результат" << endl << endl;

    printAST(tree);

    cout << endl;

    if (parser.success()) {
        cout << "Синтаксический анализ завершён успешно. Ошибок не найдено." << endl;
    }
    else {
        cout << "Синтаксический анализ завершён с ошибками." << endl;
    }

    return 0;
}