#include <iostream>
using namespace std;
int add(int a, int b) {
int result = a + b;
return result;
}
int main() {
int a = 5;
int b = 3;
int sum = 0;
int i = 0;
bool more = false;
sum = add(a, b);
more = sum > 5 && a < 10;
if (more) {
cout << "Sum is bigger than 5" << endl;
}
else {
cout << "Sum is not bigger than 5" << endl;
}
for (i = 0; i < 3; i = i + 1) {
cout << "for: " << i << endl;
}
i = 0;
while (i < 2) {
cout << "while: " << i << endl;
i = i + 1;
}
cout << "a = " << a << endl;
cout << "b = " << b << endl;
cout << "sum = " << sum << endl;
return 0;
}
