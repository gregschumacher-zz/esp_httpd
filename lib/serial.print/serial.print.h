#ifndef SERIAL_PRINT_H
#define SERIAL_PRINT_H

// #define ANSI_RED    \\033[1;31m
// #define ANSI_GREEN  \\033[1;32m
// #define ANSI_YELLOW \\033[1;33m
// #define ANSI_BLUE   \\033[1;34m
// #define ANSI_NONE   \\033[m

// #define _NL_        \r\n

#ifdef NO_PRINT
  #define SB(arg)
  #define SA() true
  #define SP(args...)
  #define SPN(args...)
  #define SPF(args...)
  #define SPFN(args...)
#else
  #define SB(arg) Serial.begin(arg)
  #define SA() Serial.available()
  #define SP(args...) Serial.print(args)
  #define SPN(args...) Serial.println(args)
  #define SPF(args...) Serial.printf(args)
  #define SPFN(args...) Serial.printf(args);Serial.println("")
#endif

#endif
