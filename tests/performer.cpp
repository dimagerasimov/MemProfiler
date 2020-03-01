#include <iostream>
#include <ctime>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return -1;
    }

    int cycles_num;
    try {
        cycles_num = std::stoi(argv[1]);
    }
    catch (std::invalid_argument) {
        cycles_num = 0;
    }

    std::clock_t begin = std::clock();
    for (int i = 0; i < cycles_num; i++) {
        delete (new int(i));
    }
    std::clock_t end = std::clock();

    std::cout << "Elapsed time: " << (end - begin) << std::endl;

    return 0;
}
