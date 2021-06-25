struct node{
		uint16_t val;
		uint32_t partial_sum;
		uint8_t my_row;
		uint8_t my_col;
		int8_t adj_left_down_col_idx;
		int8_t adj_right_down_col_idx;
};

struct node x[15][15];

x[14][0].val = 4;
x[14][0].partial_sum = 4;
x[14][0].my_row = 14;
x[14][0].my_col = 0;
x[14][0].adj_left_down_col_idx = -1;
x[14][0].adj_right_down_col_idx = -1;

row=13, col 0

partial_sum( int row, int col ){
		x[row][col].partial_sum = 
