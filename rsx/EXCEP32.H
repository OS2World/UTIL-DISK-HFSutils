/* Copyright	Rainer Schnitker 92,93 */

void excep0_386(void);	      /* label for exceptions  0 */
void excep1_386(void);	      /* label for exceptions  1 */
void excep2_386(void);	      /* label for exceptions  2 */
void excep3_386(void);	      /* label for exceptions  3 */
void excep4_386(void);	      /* label for exceptions  4 */
void excep5_386(void);	      /* label for exceptions  5 */
void excep6_386(void);	      /* label for exceptions  6 */
void excep7_386(void);	      /* label for exceptions  7 */
void excep8_386(void);	      /* label for exceptions  8 */
void excep9_386(void);	      /* label for exceptions  9 */
void excep10_386(void);       /* label for exceptions 10 */
void excep11_386(void);       /* label for exceptions 11 */
void excep12_386(void);       /* label for exceptions 12 */
void excep13_386(void);       /* label for exceptions 13 */
void excep14_386(void);       /* label for exceptions 14 */
void excep15_386(void);       /* label for exceptions 15 */
void excep16_386(void);       /* label for exceptions 16 */
void excep17_386(void);       /* label for exceptions 17 */

void extender_ds(void);       /* label int text for ds */

void	clearregs(void);	    /* clear high bit of e?x */
int	page_fault(void);	    /* page fault handler */
void	prot_cbrk ( void );
void	crit_error ( void );
void	timer_handler ( void );
long    int1Cvoff ( void );     /* label to fnct that calls old int1Ch */
long    int1Cvsel ( void );     /* label to fnct that calls old int1Ch */

extern volatile unsigned int cbrkcall;
extern volatile unsigned long time_tic;
