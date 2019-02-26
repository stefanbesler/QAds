#ifndef PLCDEF_H
#define PLCDEF_H

// C++ representation of PLC datatypes. For a big scale project
// something like this should be generated code. Otherwise,
// it is hard to keep up with changes to structs in a PLC and you
// might run into huge problems, when the PLC programmer decides
// to change the arangment of types within structs;
// Be aware of the correct alignment, you might have to change the
// alignment if you use a different compiler than the author

namespace Plc
{

enum ColorEnum
{
    red,
    green,
    blue
};

struct ExampleSubSubStruct
{
    short integer1;
    short integer2;
    float real1;
    ColorEnum color;
};

struct ExampleSubStruct
{
    short integer1;
    ExampleSubSubStruct sub1;
    int integer2;
    ExampleSubSubStruct sub2;
    float real1;
    double lreal1;
    ExampleSubSubStruct sub3;
};

struct ExampleStruct
{
    char string1[256];
    char string2[81];
    ExampleSubStruct sub1;
};
}

#endif // PLCDEF_H
