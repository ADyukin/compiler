#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>

using namespace std;

bool checkMultilineComments(string code) {
    int openCount = 0;
    int closeCount = 0;

    for (int i = 0; i < code.length() - 1; i++) {
        if (code[i] == '/' && code[i + 1] == '*') {
            openCount++;
        }

        if (code[i] == '*' && code[i + 1] == '/') {
            closeCount++;
        }
    }

    if (openCount > closeCount) {
        cout << "Ошибка: найден незакрытый многострочный комментарий\n";
        return false;
    }

    if (closeCount > openCount) {
        cout << "Ошибка: найдено закрытие комментария без открытия\n";
        return false;
    }

    return true;
}

bool checkInvalidCharacters(string code) {
    for (int i = 0; i < code.length(); i++) {
        unsigned char ch = code[i];

        if (!(ch == '\n' || ch == '\r' || ch == '\t' || ch >= 32)) {
            cout << "Ошибка: найден недопустимый символ в позиции " << i << endl;
            return false;
        }
    }

    return true;
}

string removeComments(string code) {
    regex multiComment(R"(/\*[\s\S]*?\*/)");
    code = regex_replace(code, multiComment, "");

    regex singleComment(R"(//[^\n\r]*)");
    code = regex_replace(code, singleComment, "");

    return code;
}

string removeExtraSpaces(string code) {
    stringstream input(code);
    string line;
    string result;

    regex startSpaces(R"(^[ \t]+)");
    regex endSpaces(R"([ \t]+$)");
    regex manySpaces(R"([ \t]{2,})");

    while (getline(input, line)) {
        line = regex_replace(line, startSpaces, "");
        line = regex_replace(line, endSpaces, "");
        line = regex_replace(line, manySpaces, " ");

        if (line != "") {
            result += line + "\n";
        }
    }

    return result;
}

int main() {
    setlocale(LC_ALL, "");

    string inputFile = "test.cpp";
    string outputFile = "result.cpp";

    ifstream fin(inputFile);

    if (!fin.is_open()) {
        cout << "Ошибка: не удалось открыть файл " << inputFile << endl;
        return 1;
    }

    string code = "";
    string line;

    while (getline(fin, line)) {
        code += line + "\n";
    }

    fin.close();

    if (!checkInvalidCharacters(code)) {
        return 1;
    }

    if (!checkMultilineComments(code)) {
        return 1;
    }

    string result = removeComments(code);
    result = removeExtraSpaces(result);

    ofstream fout(outputFile);

    if (!fout.is_open()) {
        cout << "Ошибка: не удалось создать файл " << outputFile << endl;
        return 1;
    }

    fout << result;
    fout.close();

    cout << "Результат очистки:\n";
    cout << result << endl;

    cout << "Ошибок не выявлено\n";
    cout << "Очищенный код записан в файл " << outputFile << endl;

    return 0;
}