# Fei Bytecode Interpreter

A bytecode interpreter and virtual machine for Fei, a dynamically typed, high-level, object-oriented programming language

## Fei Syntax

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

### Control Flow
#### If statements
> 'else if' can be replaced with the 'elf' keyword/
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
#### While and For loops
> Fei inherits while and for loops from C
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
#### Do While and Repeat Until
> Fei also supports do while and repeat until loops. 'do while' loops until the expression is false. 'repeat 'until' loops until the expression is true
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
> Switch cases automatically breaks if the condition is met
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
> All loops support break and continue statements
```
while condition
{
    if breakCondition then break;
    if continueCondition then continue;
}
```

> Heavily inspired by the Lox Programming Language http://www.craftinginterpreters.com/
