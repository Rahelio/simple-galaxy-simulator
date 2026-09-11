# simple-galaxy-simulator
a simple milkyway simluator in c/c++

# Macos
```  g++ -std=c++17 -o galaxy galaxy.cpp $(pkg-config --cflags --libs gtk4) -lm ```

# Linux
``` gcc -std=c++17 `pkg-config --cflags --libs gtk4` -o galaxy galaxy.cpp -lm ```

### To run
./galaxy