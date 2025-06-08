#include <INTRINS.H>

void Delay(unsigned int xms) 
{
        while(xms)
        {
            unsigned char i, j;
    
            i = 2;
            j = 199;
            do
            {
                    while (--j);
            } while (--i);
            
            xms--;
        }
}