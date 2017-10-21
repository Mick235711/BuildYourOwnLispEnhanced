# BuildYourOwnLisp
### My Own Lisp Interpreter in C

## Website
I write the code by looking at [Build Your Own Lisp](http://buildyourownlisp.com)

## Usage
    cd <dir>
    ./<dir>

## Latest Version
look at `double` dir

Lispy v1.1: `double`, add double number support

Lispy v1.0: `lispy1`, first stable version with string support

## Library
In `library` dir, there is `prelude.lspy`

## Usage
    $ ./utils
    Lispy Version 0.0.0.1.2
    Press quit 0 to Exit
    
    lispy> + 2 3
    5
    lispy> 

Just enter whatever you want!

Type `quit 0` to exit

## Feature

inter-polish expression (`+ 2 3`, `== {} {}`)

load file (`lispy> load "<filename>"`)

run file (just pass as command line argument)

## Compile yourself
The binary is already compiled

#### Dependencies
`libedit`

`mpc` http://github.com/orangeduck/mpc
(Already included in `mpc` dir)

to compile yourself, enter in some `<dir>`:

    cc -std=c99 -Wall -ledit -I../mpc <dir>.c ../mpc/mpc.c -o <dir>
