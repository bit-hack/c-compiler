// check that:
// - assignments nest correctly
// - left hand side is lvalue
// - right hand side is rvalue
// - assignment decays to rvalue

int main() {
    int a;
    int b;
    int c;
    a = b = c;
}
