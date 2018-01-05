#include "credb/Witness.h"
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
    if(argc != 3)
    {
        std::cout << "usage: " << argv[0] << " public_key.asc witness.asc" << std::endl;
        return 1;
    }

    std::ifstream fin;

    fin.open(argv[1]);
    if(!fin)
    {
        std::cout << "Failed to open file: " << argv[1];
        return 1;
    }

    std::string pkey;
    std::getline(fin, pkey);
    fin.close();

    fin.open(argv[2]);
    if(!fin)
    {
        std::cout << "Failed to open file: " << argv[1];
        return 1;
    }

    try
    {
        credb::Witness witness;
        fin >> witness;
        if(!witness.is_valid(pkey))
        {
            std::cout << "The witness is not valid!" << std::endl;
            return 2;
        }
        std::cout << "The witness is valid. Content:" << std::endl;
        std::cout << witness.pretty_print_content(2) << std::endl;
    }
    catch(const std::runtime_error &e)
    {
        std::cout << e.what() << std::endl;
        return 3;
    }
}
