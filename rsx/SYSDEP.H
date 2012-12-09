void	save_emu_state(NEWPROCESS *proc);
void	load_emu_state(NEWPROCESS *proc);
int	install_rsx387(void);
void	get_emxl_psp(unsigned);
void	build_emx_args(int *, char ***);
void	build_dj_args(int *, char ***);
void	set_stdout(void);
void	flush_all_buffers(void);

extern UINT emu_sel;
