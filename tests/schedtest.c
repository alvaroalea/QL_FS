/* schedtest.c - el planificador por interrupciones (-DUSE_SCHED) en PC: despegue en 40 s simulados con la interrupcion simulada
 * (host_tick llama a la funcion enganchada), pasos de fisica por segundo y capturas. Compilar con -DUSE_SCHED. */
#include "../src/sim.h"
#include "../src/platform.h"
#include <stdio.h>

extern u32 host_ticks;
void host_tick(void);
void host_key(int k, int down);
int host_dump(const char *fname);

int main(void)
{
    char acf[64];
    int t, calls = 0, rot = 0;
    u32 n0;
    plat_path(acf, "mono.acf");
    if (sim_init("build/iberia.scn", acf, 12)) { printf("no carga\n"); return 1; }
    host_ticks = 1000;
    sim.f.thr[0] = FX_ONE;
    host_key(KEY_E, 1); host_key(KEY_F4, 1);      /* arrancador y gases a fondo (por teclado: pasa por K y controls_step) */
    n0 = sim.nsteps;
    for (t = 0; t < 50 * 60; t++) {              /* 60 s */
        host_tick();
        sim_frame(); calls++;
        if (t == 50 * 6) host_key(KEY_E, 0);
        if (!rot && sim.f.ias > 55 * 33000) { rot = t; host_key(KEY_DOWN, 1); }   /* tirar de la palanca al pasar de 55 kt, 1,5 s */
        if (rot && t == rot + 75) host_key(KEY_DOWN, 0);
    }
    printf("pasos de fisica: %lu en 60 s = %.2f/s;  reloj +%lu s;  IAS %ld kt  AGL %ld m  %s\n", (unsigned long)(sim.nsteps - n0),
           (sim.nsteps - n0) / 60.0, (unsigned long)(sim.clock - 12 * 3600UL), (long)(sim.f.ias / 65536.0 * 1.9438), (long)(sim.f.agl >> 16),
           sim.f.on_ground ? "EN SUELO" : "EN EL AIRE");
    printf("vistas completas %d, vueltas del panel por la cola de turnos ok\n", sim.fps_v);
    host_dump("build/shots/sched.scr");
    sim_stop();
    return 0;
}
