/*-
 * Copyright (c) 2012 Ilya Kaliman
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>

#include "efp_private.h"

#define DR_A      (dr[a])
#define DR_B      (dr[b])
#define DR_C      (dr[c])

#define Q1        (pt_i->monopole)
#define Q2        (pt_j->monopole)
#define D1_A      (pt_i->dipole[a])
#define D2_A      (pt_j->dipole[a])
#define D1_B      (pt_i->dipole[b])
#define D2_B      (pt_j->dipole[b])
#define Q1_AB     (pt_i->quadrupole[t2_idx(a, b)])
#define Q2_AB     (pt_j->quadrupole[t2_idx(a, b)])
#define O1_ABC    (pt_i->octupole[t3_idx(a, b, c)])
#define O2_ABC    (pt_j->octupole[t3_idx(a, b, c)])

#define SUM_A(x)  (a = 0, tmp_a = x, a = 1, tmp_a += x, a = 2, tmp_a += x)
#define SUM_B(x)  (b = 0, tmp_b = x, b = 1, tmp_b += x, b = 2, tmp_b += x)
#define SUM_C(x)  (c = 0, tmp_c = x, c = 1, tmp_c += x, c = 2, tmp_c += x)

static inline double
get_damp_screen(struct efp *efp, double r_ij, double pi, double pj)
{
	if (fabs((pi - pj) * r_ij) < 1.0e-5)
		return 1.0 - (1.0 + 0.5 * pi * r_ij) * exp(-pi * r_ij);
	else
		return 1.0 - exp(-pi * r_ij) * pj * pj / (pj * pj - pi * pi) -
			     exp(-pj * r_ij) * pi * pi / (pi * pi - pj * pj);
}

static double
compute_elec_pt(struct efp *efp, int i, int j, int ii, int jj)
{
	double energy = 0.0;

	struct frag *fr_i = efp->frags + i;
	struct frag *fr_j = efp->frags + j;

	struct multipole_pt *pt_i = fr_i->multipole_pts + ii;
	struct multipole_pt *pt_j = fr_j->multipole_pts + jj;

	double dr[3] = {
		pt_j->x - pt_i->x, pt_j->y - pt_i->y, pt_j->z - pt_i->z
	};

	double r = vec_len(VEC(dr[0]));

	double ri[10];
	powers(1.0 / r, 10, ri);

	int a, b, c;
	double tmp_a, tmp_b, tmp_c;
	double t1, t2, t3, t4;

	/* monopole - monopole */
	t1 = ri[1] * Q1 * Q2;

	if (efp->opts.elec_damp == EFP_ELEC_DAMP_SCREEN)
		t1 *= get_damp_screen(efp, r, fr_i->screen_params[ii],
					fr_j->screen_params[jj]);

	energy += t1;

	/* monopole - dipole */
	energy += ri[3] * Q2 * SUM_A(D1_A * DR_A);
	energy -= ri[3] * Q1 * SUM_A(D2_A * DR_A);

	/* monopole - quadrupole */
	energy += ri[5] * Q2 * SUM_A(SUM_B(Q1_AB * DR_A * DR_B));
	energy += ri[5] * Q1 * SUM_A(SUM_B(Q2_AB * DR_A * DR_B));

	/* monopole - octupole */
	energy += ri[7] * Q2 * SUM_A(SUM_B(SUM_C(O1_ABC * DR_A * DR_B * DR_C)));
	energy -= ri[7] * Q1 * SUM_A(SUM_B(SUM_C(O2_ABC * DR_A * DR_B * DR_C)));

	/* dipole - dipole */
	energy += ri[3] * SUM_A(D1_A * D2_A);
	energy -= 3 * ri[5] * SUM_A(SUM_B(D1_A * D2_B * DR_A * DR_B));

	/* dipole - quadrupole */
	energy += 2 * ri[5] * SUM_A(SUM_B(Q1_AB * D2_A * DR_B));
	energy -= 2 * ri[5] * SUM_A(SUM_B(Q2_AB * D1_A * DR_B));

	t1 = SUM_A(D2_A * DR_A);
	energy -= 5 * ri[7] * SUM_A(SUM_B(Q1_AB * DR_A * DR_B)) * t1;

	t1 = SUM_A(D1_A * DR_A);
	energy += 5 * ri[7] * SUM_A(SUM_B(Q2_AB * DR_A * DR_B)) * t1;

	/* quadrupole - quadrupole */
	t1 = 0.0;
	for (b = 0; b < 3; b++) {
		t2 = SUM_A(Q1_AB * DR_A);
		t3 = SUM_A(Q2_AB * DR_A);
		t1 += t2 * t3;
	}
	t2 = SUM_A(SUM_B(Q1_AB * DR_A * DR_B));
	t3 = SUM_A(SUM_B(Q2_AB * DR_A * DR_B));
	t4 = SUM_A(SUM_B(Q1_AB * Q2_AB));

	energy -= 20 * ri[7] * t1 / 3.0;
	energy += 35 * ri[9] * t2 * t3 / 3.0;
	energy +=  2 * ri[5] * t4 / 3.0;

	/* gradient */
	if (efp->grad) {
	}
	return energy;
}

static double
compute_elec_frag(struct efp *efp, int i, int j)
{
	double energy = 0.0;

	struct frag *fr_i = efp->frags + i;
	struct frag *fr_j = efp->frags + j;

	for (int ii = 0; ii < fr_i->n_multipole_pts; ii++)
		for (int jj = 0; jj < fr_j->n_multipole_pts; jj++)
			energy += compute_elec_pt(efp, i, j, ii, jj);

	return energy;
}

enum efp_result
efp_compute_elec(struct efp *efp)
{
	if (efp->grad)
		return EFP_RESULT_NOT_IMPLEMENTED;

	double energy = 0.0;

	for (int i = 0; i < efp->n_frag; i++)
		for (int j = i + 1; j < efp->n_frag; j++)
			energy += compute_elec_frag(efp, i, j);

	efp->energy[efp_get_term_index(EFP_TERM_ELEC)] = energy;
	return EFP_RESULT_SUCCESS;
}

static void
rotate_quad(const struct mat *rotmat, const double *in, double *out)
{
	rotate_t2(rotmat, in, out);

	out[1] /= 2.0; /* xy */
	out[2] /= 2.0; /* xz */
	out[4] /= 2.0; /* yz */
}

static void
rotate_oct(const struct mat *rotmat, const double *in, double *out)
{
	rotate_t3(rotmat, in, out);

	out[1] /= 3.0; /* xxy */
	out[2] /= 3.0; /* xxz */
	out[3] /= 3.0; /* xyy */
	out[4] /= 6.0; /* xyz */
	out[5] /= 3.0; /* xzz */
	out[7] /= 3.0; /* yyz */
	out[8] /= 3.0; /* yzz */
}

void
efp_update_elec(struct frag *frag, const struct mat *rotmat)
{
	for (int i = 0; i < frag->n_multipole_pts; i++) {
		/* move point position */
		move_pt(VEC(frag->x), rotmat,
			VEC(frag->lib->multipole_pts[i].x),
			VEC(frag->multipole_pts[i].x));

		/* rotate dipole */
		rotate_t1(rotmat, frag->lib->multipole_pts[i].dipole,
				frag->multipole_pts[i].dipole);

		/* rotate quadrupole */
		rotate_quad(rotmat, frag->lib->multipole_pts[i].quadrupole,
				frag->multipole_pts[i].quadrupole);

		/* correction for Buckingham quadrupoles XXX */
		double *quad = frag->multipole_pts[i].quadrupole;

		double qtr = quad[t2_idx(0, 0)] +
			     quad[t2_idx(1, 1)] +
			     quad[t2_idx(2, 2)];

		quad[0] = 1.5 * quad[0] - 0.5 * qtr; /* xx */
		quad[1] = 1.5 * quad[1];
		quad[2] = 1.5 * quad[2];
		quad[3] = 1.5 * quad[3] - 0.5 * qtr; /* yy */
		quad[4] = 1.5 * quad[4];
		quad[5] = 1.5 * quad[5] - 0.5 * qtr; /* zz */

		/* rotate octupole */
		rotate_oct(rotmat, frag->lib->multipole_pts[i].octupole,
				frag->multipole_pts[i].octupole);

		/* correction for Buckingham octupoles XXX */
		double *oct = frag->multipole_pts[i].octupole;

		double otrx = oct[t3_idx(0, 0, 0)] +
			      oct[t3_idx(0, 1, 1)] +
			      oct[t3_idx(0, 2, 2)];
		double otry = oct[t3_idx(0, 0, 1)] +
			      oct[t3_idx(1, 1, 1)] +
			      oct[t3_idx(1, 2, 2)];
		double otrz = oct[t3_idx(0, 0, 2)] +
			      oct[t3_idx(1, 1, 2)] +
			      oct[t3_idx(2, 2, 2)];

		oct[0] = 2.5 * oct[0] - 1.5 * otrx; /* xxx */
		oct[1] = 2.5 * oct[1] - 0.5 * otry;
		oct[2] = 2.5 * oct[2] - 0.5 * otrz;
		oct[3] = 2.5 * oct[3] - 0.5 * otrx;
		oct[4] = 2.5 * oct[4];
		oct[5] = 2.5 * oct[5] - 0.5 * otrx;
		oct[6] = 2.5 * oct[6] - 1.5 * otry; /* yyy */
		oct[7] = 2.5 * oct[7] - 0.5 * otrz;
		oct[8] = 2.5 * oct[8] - 0.5 * otry;
		oct[9] = 2.5 * oct[9] - 1.5 * otrz; /* zzz */
	}
}
