# Bytecode Interpreter

A bytecode interpreter and virtual machine for a dynamically typed, high-level, object-oriented programming language

> Largely implemented/referenced from the Lox Programming Language http://www.craftinginterpreters.com/
 
## The Program

The interpreter is composed of three main parts: the scanner/lexer, the compiler and the virtual machine
- Scanner: converts syntax into tokens
- Compiler: parses syntax tokens into bytes/opcodes
- Virtual machine: reads bytecode and executes instructions


## Language Syntax

### Console Output
```
print "Hello world";
```

### Variable declarations and assignments
> Fei supports three native data types: strings, numbers and booleans.
> The '=' assignment operator can be replaced with the 'assigned' keyword
```
var myInt;
myInt assigned value;
var myString = "string";
var myBool = true;
```

### Comparators and Logical Operators
> The '==' comparison operator can be replaced with the keywords 'is' or 'equals'
```
40 > 41;       // false
194.5 <= 992.2;   // true

var string assigned "isString";
string == "isString";         // true
string is "notString";        // false

false = !true;    // true
```

### Arithmetic Operators
> Fei supports the 4 main binary operators as well as the modulo operator
```
a + b;
24 / 12;
78 % 13;
```

### Control Flow
#### If Statements
> 'else if' can be replaced with the 'elf' keyword.
```
if condition then
{
    print "then statement";
}
elf condition then
{
  print "else if statement";
}
else
{
  print "else statement";
}
```
#### While and For Loops
> Fei inherits while and for loops from C.
```
while condition
{
  print "while statement";
}

for (var i = 0; i < 10; i = i + 1)
{
  print i;
}
```
#### Do While and Repeat Until Loops
> Fei also supports do while and repeat until loops. 'do while' loops until the expression is false. 'repeat 'until' loops until the expression is true.
```
var i assigned 0;

do
{
  print i;
  i= i + 1;
} while i < 10;
 
repeat
{
  print i;
  i = i - 1;
} until i equals 0;
```

#### Switch Statements
> Switch cases automatically breaks if the condition is met.
```
switch variable
{
  case 0:
  {
      // execute statment
  }
   case 1:
  {
      // execute statment
  }
  default:
    // default statmenet
}
```

#### Break and Continue Statements
> All loops support break and continue statements.
```
while condition
{
    if breakCondition then break;
    if continueCondition then continue;
}
```

### Functions
```
function product(a, b)
{
    return a * b;
}

function printHello()
{
    print "Hello";
}

var prod = product(12, 23);      // 276
printHello();                    // prints "Hello"
```

### Classes
> Fei has a Python-like class system. Constructors are initialize with the 'init' keyword and class members use the 'this' keyword. 
> The keyword 'from ' used to declare inheritance. The 'super' keyword is used for superclasses.

```
class ParentClass
{
    init(name)
    {
        this.name = name;       // assign name to class member 'name'
    }
    
    printName()                 // class methods are initialized without the 'function' keyword
    {
        print this.name;        
    }
}

ParentClass parent("My name");
parent.printName();             // prints "My name"

class ChildClass from ParentClass
{
    init(name, age)
    {
        super.init(name);       // parent constructor
        this.age = age;
    }
    
    printAge()
    {
        print this.age;
    }
}

ChildClass child("Your name", 17);
child.printName();             // prints "Your name"
child.printAge();              // prints 17

```



