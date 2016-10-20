#define F (1 << 14) /* fixed point 1 */
#define INT_MAX ((1 << 31) - 1) 
#define INT_MIN (-(1 << 31)) 

/* x and y denote fixed_point numbers in 17.14 format */
/* n is an integer */

int int_to_fp(int n);						/* Change integer to fixed point */
int fp_to_int_round(int x);			/* Change FP to int(round) */
int fp_to_int(int x);						/* Change FP to int */
int add_fp(int x, int y);				/* Addition FP */
int add_mixed(int x, int n);	  /* Addition FP and int */
int sub_fp(int x, int y);				/* Subtraction FP X-Y */
int sub_mixed(int x, int n);		/* Subtraction FP and int X-N */ 
int mult_fp(int x, int y);			/* Multiplication FP */
int mult_mixed(int x, int y);   /* Multiplication FP and int */
int div_fp(int x, int y);				/* Divid FP X/Y */
int div_mixed(int x, int n);		/* Divid FP and int X/N */


int int_to_fp(int n) 
{
	return n * F;
}

int fp_to_int_round(int x)
{
	
	if(x >= 0)
		return (x+F / 2) / F;
	else 
		return (x-F / 2) / F; 
	
}

int fp_to_int(int x)
{
	return x / F;
}

int add_fp(int x, int y)
{
	return x + y;
}

int add_mixed(int x, int n)
{
	return x + n * F;
}

int sub_fp(int x, int y)
{
	return x - y;
}

int sub_mixed(int x, int n)
{
	return x - n * F;
}

int mult_fp(int x, int y)
{
	return ((int64_t)x) * y / F;
}

int mult_mixed(int x, int n)
{
	return x * n;
}

int div_fp(int x, int y)
{
	return ((int64_t)x) * F / y; 
}

int div_mixed(int x, int n)
{
	return x / n;
}

