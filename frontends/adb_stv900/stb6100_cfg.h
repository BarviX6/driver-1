/************************************************************************
 *
 * STB6100 Silicon Tuner
 * Copyright (C) Manu Abraham (abraham.manu@gmail.com)
 *
 * Copyright (C) ST Microelectronics
 *
 * Version for ADB ITI-5800SX, BZZB model
 *
 * Tuners are directkly connected to the I2C bus, not through
 * the STV0900 I2C gate.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ************************************************************************/

static int stb6100_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct dvb_frontend_ops	*frontend_ops = NULL;
	struct dvb_tuner_ops	*tuner_ops = NULL;
	u32 freq = 0;
	int err = 0;

	if (&fe->ops)
	{
		frontend_ops = &fe->ops;
	}
	if (&frontend_ops->tuner_ops)
	{
		tuner_ops = &frontend_ops->tuner_ops;
	}
	if (tuner_ops->get_frequency)
	{
		if ((err = tuner_ops->get_frequency(fe, frequency)) < 0)
		{
			dprintk(1,"%s: Invalid parameter\n", __func__);
			return err;
		}
		freq = *frequency;
//		dprintk(50, "%s: Frequency=%d\n", __func__, freq);

	}
	return 0;
}

static int stb6100_set_frequency(struct dvb_frontend *fe, u32 frequency)
{
	struct dvb_frontend_ops	*frontend_ops = NULL;
	struct dvb_tuner_ops	*tuner_ops = NULL;
	int err = 0;

	if (&fe->ops)
	{
		frontend_ops = &fe->ops;
	}
	if (&frontend_ops->tuner_ops)
	{
		tuner_ops = &frontend_ops->tuner_ops;
	}
	if (tuner_ops->set_frequency)
	{
		if ((err = tuner_ops->set_frequency(fe, frequency)) < 0)
		{
			dprintk(1, "%s: Invalid parameter\n", __func__);
			return err;
		}
//		dprintk(50, "%s: Frequency=%d\n", __func__, frequency);
	}
	return 0;
}

static int stb6100_get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	struct dvb_frontend_ops	*frontend_ops = NULL;
	struct dvb_tuner_ops	*tuner_ops = NULL;
	u32 band = 0;
	int err = 0;

	if (&fe->ops)
	{
		frontend_ops = &fe->ops;
	}
	if (&frontend_ops->tuner_ops)
	{
		tuner_ops = &frontend_ops->tuner_ops;
	}
	if (tuner_ops->get_bandwidth)
	{
		if ((err = tuner_ops->get_bandwidth(fe, bandwidth)) < 0)
		{
			dprintk(1, "%s: Invalid parameter\n", __func__);
			return err;
		}
		band = *bandwidth;
	}
//	dprintk(50, "%s: Bandwidth=%d\n", __func__, band);
	return 0;
}

static int stb6100_set_bandwidth(struct dvb_frontend *fe, u32 bandwidth)
{
	struct dvb_frontend_ops	*frontend_ops = NULL;
	struct dvb_tuner_ops	*tuner_ops = NULL;
	int err = 0;

	if (&fe->ops)
	{
		frontend_ops = &fe->ops;
	}
	if (&frontend_ops->tuner_ops)
	{
		tuner_ops = &frontend_ops->tuner_ops;
	}
	if (tuner_ops->set_bandwidth)
	{
		if ((err = tuner_ops->set_bandwidth(fe, bandwidth)) < 0)
		{
			dprintk(1, "%s: Invalid parameter\n", __func__);
			return err;
		}
//		dprintk(50, "%s: Bandwidth=%d\n", __func__, bandwidth);
	}
	return 0;
}
// vim:ts=4
