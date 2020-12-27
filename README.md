# Fei Bytecode Interpreter

A bytecode interpreter and virtual machine for Fei, a dynamically typed, high-level, object-oriented programming language

## Fei Syntax

### Console Output
```
print "Hello world";
```

### Variable declarations and assignments
> Fei supports three native data types: strings, numbers and booleans
> the '=' assignment operator can be replaced with the 'assigned' keyword
```
var myInt;
myInt assigned value;
var myString = "string";
var myBool = true;
```

### Comparators
> The '==' comparison operator can be replaced with the keywords 'is' or 'equals'
```
40 > 41;       // false
194.5 <= 992.2;   // true

var string assigned "isString";
string == "isString";         // true
string is "notString";        // false

false = !true;    // true
```


> Heavily inspired by the Lox Programming Language http://www.craftinginterpreters.com/
