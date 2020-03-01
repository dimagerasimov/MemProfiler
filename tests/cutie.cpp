class A {
public:
    A() { var = new int(0xAA); }
    ~A() { delete var; };

private:
    int *var;
};

class B {
public:
    B() { var = new int(0xBB); }
    ~B() { };

private:
    int *var;
};

A a;
B b;

int main(int argc, char *argv[]) {
    return 0;
}
