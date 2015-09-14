#include <iostream>

int main() {
  
  int x = 10;
  double y, z = 5, w;
  
  for (int i = 0; i < x; i++) {
    for (int j = 100; j != 0; j--) {
      x = x * 2;
      
      if (i % 2) {
	y++;
	w = z + 4;
	z = z + 203;
	y = y + z;
      } else {
	y = y + 4;
      }
      z = z + w;
    }
  }
  
  z = w + y;
  
  return 0;
}
