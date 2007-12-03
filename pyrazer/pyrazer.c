/*
 *   Razer device access python module
 *
 *   Copyright (C) 2007 Michael Buesch <mb@bu3sch.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <Python.h>
#include <errno.h>

#include "librazer.h"


static PyObject *pyrazer_except;

static void raise_errno_exception(int err)
{
	int old_errno;

	if (err < 0)
		err = -err;
	old_errno = errno;
	errno = err;
	PyErr_SetFromErrno(pyrazer_except);
	errno = old_errno;
}

/*****************************************************************************
 * LED-Object definitions                                                    *
 *****************************************************************************/

struct pyrazer_led {
	PyObject_HEAD
	struct razer_led *d;
};

static PyObject * method_led_get_name(PyObject *self, PyObject *args)
{
	struct pyrazer_led *led = (struct pyrazer_led *)self;

	return PyString_FromString(led->d->name);
}

static PyObject * method_led_get_id(PyObject *self, PyObject *args)
{
	struct pyrazer_led *led = (struct pyrazer_led *)self;

	return PyInt_FromLong(led->d->id);
}

static PyObject * method_led_get_state(PyObject *self, PyObject *args)
{
	struct pyrazer_led *led = (struct pyrazer_led *)self;

	return PyInt_FromLong(led->d->state);
}

static PyObject * method_led_toggle_state(PyObject *self, PyObject *args)
{
	struct pyrazer_led *led = (struct pyrazer_led *)self;
	int err, new_state;

	if (!PyArg_ParseTuple(args, "i", &new_state))
		return NULL;
	err = led->d->toggle_state(led->d, new_state);
	if (err) {
		raise_errno_exception(err);
		return NULL;
	}

	return Py_None;
}

static PyMethodDef pyrazer_led_methods[] = {
	{ "getName", method_led_get_name, METH_NOARGS,
	  "Get the LED name string" },
	{ "getId", method_led_get_id, METH_NOARGS,
	  "Get the LED ID number" },
	{ "getState", method_led_get_state, METH_NOARGS,
	  "Get the currently set LED state" },
	{ "toggleState", method_led_toggle_state, METH_VARARGS,
	  "Toggle the state" },
	{ },
};

static PyObject * pyrazer_led_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyErr_SetObject(PyExc_Exception, PyExc_NotImplementedError);
	return NULL;
}

static void pyrazer_led_free(PyObject *obj)
{
	struct pyrazer_led *l = (struct pyrazer_led *)obj;

	l->d->next = NULL; /* Only free this LED. */
	razer_free_leds(l->d);
	obj->ob_type->tp_free(obj);
}

static PyTypeObject pyrazer_led_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name	= "pyrazer.Led",
	.tp_basicsize	= sizeof(struct pyrazer_led),
	.tp_methods	= pyrazer_led_methods,
	.tp_flags	= Py_TPFLAGS_DEFAULT,
	.tp_new		= pyrazer_led_new,
	.tp_dealloc	= pyrazer_led_free,
	.tp_doc		= "A LED object",
};

/*****************************************************************************
 * Mouse-Object definitions                                                  *
 *****************************************************************************/

struct pyrazer_mouse {
	PyObject_HEAD
	struct razer_mouse *d;
};

static PyObject * method_mouse_get_busid(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;

	return PyString_FromString(m->d->busid);
}

static PyObject * method_mouse_get_type(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;

	return PyInt_FromLong(m->d->type);
}

static PyObject * method_mouse_claim(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	int err;

	err = m->d->claim(m->d);
	if (err) {
		raise_errno_exception(err);
		return NULL;
	}

	return Py_None;
}

static PyObject * method_mouse_release(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;

	m->d->release(m->d);

	return Py_None;
}

static PyObject * method_mouse_get_fw_version(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	int ver;

	ver = m->d->get_fw_version(m->d);
	if (ver < 0) {
		raise_errno_exception(ver);
		return NULL;
	}

	return PyInt_FromLong(ver);
}

static PyObject * method_mouse_get_leds(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	struct razer_led *led, *leds;
	struct pyrazer_led *pyled;
	int i, count;
	PyObject *tuple;

	count = m->d->get_leds(m->d, &leds);
	if (count < 0) {
		raise_errno_exception(count);
		return NULL;
	}
	tuple = PyTuple_New(count);
	if (!tuple) {
		razer_free_leds(leds);
		return NULL;
	}
	for (led = leds, i = 0; led; led = led->next, i++) {
		pyled = (struct pyrazer_led *)PyType_GenericAlloc(&pyrazer_led_type, 1);
		if (!pyled) {
			Py_DECREF(tuple);
			/* Free the leds that are not yet inserted into
			 * the tuple. */
			razer_free_leds(led);
			return NULL;
		}
		pyled->d = led;
		PyTuple_SET_ITEM(tuple, i, (PyObject *)pyled);
	}

	return tuple;
}

static PyObject * method_mouse_supported_freqs(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	enum razer_mouse_freq *freq_list;
	int i, count;
	PyObject *tuple, *pyint;

	count = m->d->supported_freqs(m->d, &freq_list);
	if (count < 0) {
		raise_errno_exception(count);
		return NULL;
	}
	tuple = PyTuple_New(count);
	if (!tuple) {
		razer_free_freq_list(freq_list, count);
		return NULL;
	}
	for (i = 0; i < count; i++) {
		pyint = PyInt_FromLong(freq_list[i]);
		if (!pyint) {
			razer_free_freq_list(freq_list, count);
			return NULL;
		}
		PyTuple_SET_ITEM(tuple, i, pyint);
	}
	razer_free_freq_list(freq_list, count);

	return tuple;
}

static PyObject * method_mouse_get_freq(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	enum razer_mouse_freq freq;

	freq = m->d->get_freq(m->d);

	return PyInt_FromLong(freq);
}

static PyObject * method_mouse_set_freq(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	int err, new_freq;

	if (!PyArg_ParseTuple(args, "i", &new_freq))
		return NULL;
	err = m->d->set_freq(m->d, new_freq);
	if (err) {
		raise_errno_exception(err);
		return NULL;
	}

	return Py_None;
}

static PyObject * method_mouse_supported_res(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	enum razer_mouse_res *res_list;
	int i, count;
	PyObject *tuple, *pyint;

	count = m->d->supported_resolutions(m->d, &res_list);
	if (count < 0) {
		raise_errno_exception(count);
		return NULL;
	}
	tuple = PyTuple_New(count);
	if (!tuple) {
		razer_free_resolution_list(res_list, count);
		return NULL;
	}
	for (i = 0; i < count; i++) {
		pyint = PyInt_FromLong(res_list[i]);
		if (!pyint) {
			razer_free_resolution_list(res_list, count);
			return NULL;
		}
		PyTuple_SET_ITEM(tuple, i, pyint);
	}
	razer_free_resolution_list(res_list, count);

	return tuple;
}

static PyObject * method_mouse_get_res(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	enum razer_mouse_res res;

	res = m->d->get_resolution(m->d);

	return PyInt_FromLong(res);
}

static PyObject * method_mouse_set_res(PyObject *self, PyObject *args)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)self;
	int err, new_res;

	if (!PyArg_ParseTuple(args, "i", &new_res))
		return NULL;
	err = m->d->set_resolution(m->d, new_res);
	if (err) {
		raise_errno_exception(err);
		return NULL;
	}

	return Py_None;
}

static PyMethodDef pyrazer_mouse_methods[] = {
	{ "getBusId", method_mouse_get_busid, METH_NOARGS,
	  "Get the bus ID string" },
	{ "getType", method_mouse_get_type, METH_NOARGS,
	  "Get the mouse type identifier" },
	{ "claim", method_mouse_claim, METH_NOARGS,
	  "Claim the mouse" },
	{ "release", method_mouse_release, METH_NOARGS,
	  "Release the mouse" },
	{ "getFwVersion", method_mouse_get_fw_version, METH_NOARGS,
	  "Read the firmware version from the device" },
	{ "getLeds", method_mouse_get_leds, METH_NOARGS,
	  "Get a list of LEDs" },
	{ "supportedFreqs", method_mouse_supported_freqs, METH_NOARGS,
	  "Get a list of supported frequency values" },
	{ "getFreq", method_mouse_get_freq, METH_NOARGS,
	  "Get the currently set frequency" },
	{ "setFreq", method_mouse_set_freq, METH_VARARGS,
	  "Change the frequency" },
	{ "supportedResolutions", method_mouse_supported_res, METH_NOARGS,
	  "Get a list of supported resolutions" },
	{ "getResolution", method_mouse_get_res, METH_NOARGS,
	  "Get the currently set resulution" },
	{ "setResolution", method_mouse_set_res, METH_VARARGS,
	  "Change the resolution" },
	{ },
};

static PyObject * pyrazer_mouse_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyErr_SetObject(PyExc_Exception, PyExc_NotImplementedError);
	return NULL;
}

static void pyrazer_mouse_free(PyObject *obj)
{
	struct pyrazer_mouse *m = (struct pyrazer_mouse *)obj;

	m->d->next = NULL; /* Only free this mouse. */
	razer_free_mice(m->d);
	obj->ob_type->tp_free(obj);
}

static PyTypeObject pyrazer_mouse_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name	= "pyrazer.Mouse",
	.tp_basicsize	= sizeof(struct pyrazer_mouse),
	.tp_methods	= pyrazer_mouse_methods,
	.tp_flags	= Py_TPFLAGS_DEFAULT,
	.tp_new		= pyrazer_mouse_new,
	.tp_dealloc	= pyrazer_mouse_free,
	.tp_doc		= "A mouse object",
};

/*****************************************************************************
 * API functions                                                             *
 *****************************************************************************/

static PyObject * pyrazer_init(PyObject *self, PyObject *args)
{
	int err;

	err = razer_init();
	if (err)
		raise_errno_exception(err);

	return Py_None;
}

static PyObject * pyrazer_exit(PyObject *self, PyObject *args)
{
	razer_exit();

	return Py_None;
}

static PyObject * pyrazer_scan_mice(PyObject *self, PyObject *args)
{
	int i, count;
	struct razer_mouse *list, *mouse;
	PyObject *tuple;
	struct pyrazer_mouse *pymouse;

	count = razer_scan_mice(&list);
	if (count < 0) {
		raise_errno_exception(count);
		return NULL;
	}
	tuple = PyTuple_New(count);
	if (!tuple) {
		razer_free_mice(list);
		return NULL;
	}
	for (mouse = list, i = 0; mouse; mouse = mouse->next, i++) {
		pymouse = (struct pyrazer_mouse *)PyType_GenericAlloc(&pyrazer_mouse_type, 1);
		if (!pymouse) {
			Py_DECREF(tuple);
			/* Free the mice that are not yet inserted into
			 * the tuple. */
			razer_free_mice(mouse);
			return NULL;
		}
		pymouse->d = mouse;
		PyTuple_SET_ITEM(tuple, i, (PyObject *)pymouse);
	}

	return tuple;
}

static PyMethodDef pyrazer_methods[] = {
	{ "razerInit", pyrazer_init, METH_NOARGS, "Initialize the library" },
	{ "razerExit", pyrazer_exit, METH_NOARGS, "Free the library" },
	{ "scanMice", pyrazer_scan_mice, METH_NOARGS, "Scan for mice" },
	{ },
};

/*****************************************************************************
 * Initialization                                                            *
 *****************************************************************************/

#define __stringify(x)		#x
#define stringify(x)		__stringify(x)

#define def_const(m, x)  PyModule_AddIntConstant((m), stringify(x), (x))

PyMODINIT_FUNC initpyrazer(void)
{
	PyObject *m;

	if (PyType_Ready(&pyrazer_led_type) < 0)
		return;
	if (PyType_Ready(&pyrazer_mouse_type) < 0)
		return;

	pyrazer_except = PyErr_NewException("pyrazer.RazerException",
					    PyExc_Exception, NULL);
	if (!pyrazer_except)
		return;
	Py_INCREF(pyrazer_except);

	m = Py_InitModule("pyrazer", pyrazer_methods);
	if (!m) {
		Py_DECREF(pyrazer_except);
		return;
	}

	/* LED states */
	def_const(m, RAZER_LED_OFF);
	def_const(m, RAZER_LED_ON);
	def_const(m, RAZER_LED_UNKNOWN);

	/* Mouse scan frequencies */
	def_const(m, RAZER_MOUSE_FREQ_UNKNOWN);
	def_const(m, RAZER_MOUSE_FREQ_125HZ);
	def_const(m, RAZER_MOUSE_FREQ_500HZ);
	def_const(m, RAZER_MOUSE_FREQ_1000HZ);

	/* Mouse scan resolutions */
	def_const(m, RAZER_MOUSE_RES_UNKNOWN);
	def_const(m, RAZER_MOUSE_RES_400DPI);
	def_const(m, RAZER_MOUSE_RES_450DPI);
	def_const(m, RAZER_MOUSE_RES_900DPI);
	def_const(m, RAZER_MOUSE_RES_1600DPI);
	def_const(m, RAZER_MOUSE_RES_1800DPI);

	/* Mouse types */
	def_const(m, RAZER_MOUSETYPE_DEATHADDER);
	def_const(m, RAZER_MOUSETYPE_KRAIT);
	def_const(m, RAZER_MOUSETYPE_LACHESIS);

	/* Objects */
	PyModule_AddObject(m, "RazerException", pyrazer_except);

	Py_INCREF(&pyrazer_led_type);
	PyModule_AddObject(m, "Led", (PyObject *)&pyrazer_led_type);

	Py_INCREF(&pyrazer_mouse_type);
	PyModule_AddObject(m, "Mouse", (PyObject *)&pyrazer_mouse_type);
}
