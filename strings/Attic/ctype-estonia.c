/* See on Eesti character-set, 
mis on kasutatav koos iso-8859-1(Latin1) t�hestikuga 
autor : �lo S�stra ylo@stat.vil.ee
*/

#include <global.h>
#include "m_string.h"

uchar NEAR ctype_estonia[257] = {
0,
32,32,32,32,32,32,32,32,32,40,40,40,40,40,32,32,
32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
72,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
132,132,132,132,132,132,132,132,132,132,16,16,16,16,16,16,
16,129,129,129,129,129,129,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,16,16,16,16,16,
16,130,130,130,130,130,130,2,2,2,2,2,2,2,2,2,
2,2,2,2,2,2,2,2,2,2,2,16,16,16,16,32,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
72,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,16,1,1,1,1,1,1,1,2,
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
2,2,2,2,2,2,2,16,2,2,2,2,2,2,2,2,
};

uchar NEAR to_lower_estonia[]={
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 
31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 
46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 
61, 62, 63, 64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 
107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 
120, 121, 122, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 
103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 
129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 
142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 
155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 
184, 169, 186, 171, 172, 173, 174, 191, 176, 177, 178, 179, 180, 
181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 224, 225, 
226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 
239, 240, 241, 242, 243, 244, 245, 246, 215, 248, 249, 250, 251, 
252, 253, 254, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 
233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 
246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};

uchar NEAR to_upper_estonia[]={
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 
31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 
46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 
61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 
76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 
91, 92, 93, 94, 95, 96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 
74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 
89, 90, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 
134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 
147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 
160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 
173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 168, 185, 
170, 187, 188, 189, 190, 175, 192, 193, 194, 195, 196, 197, 198, 
199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 
212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 192, 
193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 
206, 207, 208, 209, 210, 211, 212, 213, 214, 247, 216, 217, 218, 
219, 220, 221, 222, 255
};

#ifndef __WIN32__
uchar NEAR sort_order_estonia[]={
#else
uchar sort_order_estonia[]={
#endif
0, 2, 3, 4, 5, 6, 7, 8, 9, 46, 47, 48, 49, 50, 10, 11, 12, 13, 14, 15, 
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 44, 51, 52, 53, 54, 55, 
56, 39, 57, 58, 59, 93, 60, 40, 61, 62, 118, 122, 124, 126, 128, 129, 130, 
131, 132, 133, 63, 64, 94, 95, 96, 65, 66, 134, 144, 146, 152, 154, 164, 166, 
170, 172, 178, 180, 184, 190, 192, 198, 206, 208, 210, 214, 229, 232, 238, 240, 
250, 252, 221, 67, 68, 69, 70, 71, 72, 135, 145, 147, 153, 155, 165, 167, 171, 
173, 179, 181, 185, 191, 193, 199, 207, 209, 211, 215, 230, 233, 239, 241, 251, 
253, 222, 73, 74, 75, 76, 28, 1, 29, 87, 30, 90, 116, 113, 114, 31, 117, 32, 91, 
33, 78, 82, 81, 34, 85, 86, 88, 89, 115, 42, 43, 35, 231, 36, 92, 37, 79, 84, 38, 45, 
254, 102, 103, 104, 255, 77, 105, 204, 106, 212, 98, 107, 41, 108, 142, 109, 97, 125, 
127, 80, 110, 111, 112, 205, 123, 213, 99, 119, 120, 121, 143, 140, 176, 136, 148, 244, 
138, 162, 160, 150, 156, 223, 158, 168, 182, 174, 186, 219, 194, 196, 200, 202, 242, 246, 
100, 236, 188, 216, 234, 248, 225, 227, 218, 141, 177, 137, 149, 245, 139, 163, 161, 151, 
157, 224, 159, 169, 183, 175, 187, 220, 195, 197, 201, 203, 243, 247, 101, 237, 189, 217, 
235, 249, 226, 228, 83
};
