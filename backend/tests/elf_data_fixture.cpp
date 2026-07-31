extern "C" {

extern const int yarda_ro_value = 11;
int yarda_data_value = 22;
int yarda_bss_value;
thread_local int yarda_tls_value = 33;
thread_local int yarda_tls_bss;
}

int main()
{
  return yarda_ro_value + yarda_data_value + yarda_bss_value + yarda_tls_value +
         yarda_tls_bss;
}
