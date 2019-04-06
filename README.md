# evalcat
Allow you pipe in from stdin "and then" read from keyboard. A simple script engine is also included

# compilation
g++ -std=c++17 evalcat.cpp -o evalcat

# arguments
`-h, --help`    Shows help

# example

file: example.sh
```
#!/bin/bash
#HOTKEY1=text with space
#HOTKEY2=#EVAL ls %1%
#EOF

./evalcat "#EVAL $0" | ./other_program
```


at console:
```
> ./example.sh
> hello
other_program received from cin: hello
> #HOTKEY1
other_program received from cin: text with space
> #HOTKEY2 -l
other_program received from cin: evalcat
other_program received from cin: other_program
> #ABC
The macro ABC is not defined
> #EXIT
```
