/*
 *   Razer device access library
 *   Profile emulation
 *
 *   Copyright (C) 2011 Michael Buesch <mb@bu3sch.de>
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

#include "profile_emulation.h"


static int mouse_profemu_commit(struct razer_mouse_profile_emu *emu)
{
	struct razer_mouse_profile *hw_profile = emu->hw_profile;
	unsigned int active_prof_nr = emu->active_profile->nr;
	struct razer_mouse_profile_emu_data *data;
	struct razer_mouse *mouse = emu->mouse;
	unsigned int i;
	int err;

	if (WARN_ON(active_prof_nr >= ARRAY_SIZE(emu->data)))
		return -EINVAL;
	data = &emu->data[active_prof_nr];

	err = mouse->claim(mouse);
	if (err) {
		razer_error("profile emulation: Failed to claim mouse\n");
		return err;
	}

	if (hw_profile->set_dpimapping) {
		struct razer_axis *axes = NULL;

		if (mouse->supported_axes) {
			err = mouse->supported_axes(mouse, &axes);
			if (err < 0)
				goto error;
			WARN_ON(err != data->nr_dpimappings);
		}
		for (i = 0; i < data->nr_dpimappings; i++) {
			err = hw_profile->set_dpimapping(
						hw_profile,
						axes ? &axes[i] : NULL,
						data->dpimappings[i]);
			if (err)
				goto error;
		}
	}
	if (hw_profile->set_button_function) {
		struct razer_button *buttons = NULL;

		if (mouse->supported_buttons) {
			err = mouse->supported_buttons(mouse, &buttons);
			if (err < 0)
				goto error;
		}
		WARN_ON(err != data->nr_butfuncs);
		for (i = 0; i < data->nr_butfuncs; i++) {
			err = hw_profile->set_button_function(
						hw_profile,
						buttons ? &buttons[i] : NULL,
						data->butfuncs[i]);
			if (err)
				goto error;
		}
	}
	if (hw_profile->set_freq) {
		err = hw_profile->set_freq(hw_profile, data->freq);
		if (err)
			goto error;
	}

	mouse->release(mouse);
	razer_debug("profile emulation: Committed active profile\n");

	return 0;

error:
	razer_error("profile emulation: Failed to commit settings\n");
	mouse->release(mouse);

	return err;
}

static enum razer_mouse_freq mouse_profemu_get_freq(struct razer_mouse_profile *p)
{
	struct razer_mouse *mouse = p->mouse;
	struct razer_mouse_profile_emu *emu = mouse->profemu;

	if (WARN_ON(p->nr >= ARRAY_SIZE(emu->profiles)))
		return RAZER_MOUSE_FREQ_UNKNOWN;

	return emu->data[p->nr].freq;
}

static int mouse_profemu_set_freq(struct razer_mouse_profile *p,
				  enum razer_mouse_freq freq)
{
	struct razer_mouse *mouse = p->mouse;
	struct razer_mouse_profile_emu *emu = mouse->profemu;

	if (WARN_ON(p->nr >= ARRAY_SIZE(emu->profiles)))
		return -EINVAL;

	emu->data[p->nr].freq = freq;

	if (p == emu->active_profile)
		return mouse_profemu_commit(emu);
	return 0;
}

static struct razer_mouse_dpimapping * mouse_profemu_get_dpimapping(
				struct razer_mouse_profile *p,
				struct razer_axis *axis)
{
	struct razer_mouse *mouse = p->mouse;
	struct razer_mouse_profile_emu *emu = mouse->profemu;
	struct razer_mouse_profile_emu_data *data;
	unsigned int axis_index;

	if (WARN_ON(p->nr >= ARRAY_SIZE(emu->profiles)))
		return NULL;
	data = &emu->data[p->nr];
	axis_index = axis ? axis->id : 0;
	if (WARN_ON(axis_index >= ARRAY_SIZE(data->dpimappings)))
		return NULL;

	return data->dpimappings[axis_index];
}

static int mouse_profemu_set_dpimapping(struct razer_mouse_profile *p,
					struct razer_axis *axis,
					struct razer_mouse_dpimapping *d)
{
	struct razer_mouse *mouse = p->mouse;
	struct razer_mouse_profile_emu *emu = mouse->profemu;
	struct razer_mouse_profile_emu_data *data;
	unsigned int axis_index;

	if (WARN_ON(p->nr >= ARRAY_SIZE(emu->profiles)))
		return -EINVAL;
	data = &emu->data[p->nr];
	axis_index = axis ? axis->id : 0;
	if (WARN_ON(axis_index >= ARRAY_SIZE(data->dpimappings)))
		return -EINVAL;

	data->dpimappings[axis_index] = d;

	if (p == emu->active_profile)
		return mouse_profemu_commit(emu);
	return 0;
}

static struct razer_button_function * mouse_profemu_get_button_function(
				struct razer_mouse_profile *p,
				struct razer_button *b)
{
	struct razer_mouse *mouse = p->mouse;
	struct razer_mouse_profile_emu *emu = mouse->profemu;
	struct razer_mouse_profile_emu_data *data;

	if (WARN_ON(p->nr >= ARRAY_SIZE(emu->profiles)))
		return NULL;
	data = &emu->data[p->nr];
	if (WARN_ON(b->id >= ARRAY_SIZE(data->butfuncs)))
		return NULL;

	return data->butfuncs[b->id];
}

static int mouse_profemu_set_button_function(struct razer_mouse_profile *p,
					     struct razer_button *b,
					     struct razer_button_function *f)
{
	struct razer_mouse *mouse = p->mouse;
	struct razer_mouse_profile_emu *emu = mouse->profemu;
	struct razer_mouse_profile_emu_data *data;

	if (WARN_ON(p->nr >= ARRAY_SIZE(emu->profiles)))
		return -EINVAL;
	data = &emu->data[p->nr];
	if (WARN_ON(b->id >= ARRAY_SIZE(data->butfuncs)))
		return -EINVAL;

	data->butfuncs[b->id] = f;

	if (p == emu->active_profile)
		return mouse_profemu_commit(emu);
	return 0;
}

static struct razer_mouse_profile * mouse_profemu_get(struct razer_mouse *m)
{
	return &m->profemu->profiles[0];
}

static struct razer_mouse_profile * mouse_profemu_get_active(struct razer_mouse *m)
{
	return m->profemu->active_profile;
}

static int mouse_profemu_set_active(struct razer_mouse *m,
				    struct razer_mouse_profile *p)
{
	struct razer_mouse_profile_emu *emu = m->profemu;

	if (p == emu->active_profile)
		return 0;
	emu->active_profile = p;

	return mouse_profemu_commit(emu);
}

int razer_mouse_init_profile_emulation(struct razer_mouse *m)
{
	struct razer_mouse_profile_emu *emu;
	struct razer_mouse_profile_emu_data *data;
	struct razer_mouse_profile *prof, *hw_profile;
	unsigned int i, j;
	int err;
	struct razer_axis *axes = NULL;
	int nr_axes = 1;
	struct razer_button *buttons = NULL;
	int nr_buttons = 0;

	emu = zalloc(sizeof(*emu));
	if (!emu)
		return -ENOMEM;
	emu->mouse = m;

	hw_profile = m->get_active_profile(m);
	emu->hw_profile = hw_profile;
	if (WARN_ON(!emu->hw_profile))
		goto err_free;

	if (m->supported_axes) {
		nr_axes = m->supported_axes(m, &axes);
		if (WARN_ON(nr_axes < 0))
			goto err_free;
	}

	if (m->supported_buttons) {
		nr_buttons = m->supported_buttons(m, &buttons);
		if (WARN_ON(nr_buttons < 0))
			goto err_free;
	}

	for (i = 0; i < ARRAY_SIZE(emu->profiles); i++) {
		prof = &emu->profiles[i];
		data = &emu->data[i];

		prof->mouse = m;
		prof->nr = i;

		/* Assign callbacks, if the driver supports the feature. */
		if (hw_profile->get_freq)
			prof->get_freq = mouse_profemu_get_freq;
		if (hw_profile->set_freq)
			prof->set_freq = mouse_profemu_set_freq;
		if (hw_profile->get_dpimapping)
			prof->get_dpimapping = mouse_profemu_get_dpimapping;
		if (hw_profile->set_dpimapping)
			prof->set_dpimapping = mouse_profemu_set_dpimapping;
		if (hw_profile->get_button_function)
			prof->get_button_function = mouse_profemu_get_button_function;
		if (hw_profile->set_button_function)
			prof->set_button_function = mouse_profemu_set_button_function;

		/* Load initial settings */
		if (hw_profile->get_freq)
			data->freq = hw_profile->get_freq(hw_profile);
		if (hw_profile->get_dpimapping) {
			for (j = 0; j < nr_axes; j++) {
				if (WARN_ON(j >= ARRAY_SIZE(data->dpimappings)))
					break;
				data->dpimappings[j] = hw_profile->get_dpimapping(
								hw_profile,
								axes ? &axes[j] : NULL);
			}
			data->nr_dpimappings = j;
		}
		if (hw_profile->get_button_function) {
			for (j = 0; j < nr_buttons; j++) {
				if (WARN_ON(j >= ARRAY_SIZE(data->butfuncs)))
					break;
				data->butfuncs[j] = hw_profile->get_button_function(
								hw_profile,
								buttons ? &buttons[j] : NULL);
			}
			data->nr_butfuncs = j;
		}
	}
	emu->active_profile = &emu->profiles[0];

	err = mouse_profemu_commit(emu);
	if (err)
		goto err_free;

	m->nr_profiles = ARRAY_SIZE(emu->profiles);
	m->get_profiles = mouse_profemu_get;
	m->get_active_profile = mouse_profemu_get_active;
	m->set_active_profile = mouse_profemu_set_active;

	m->profemu = emu;
	m->flags |= RAZER_MOUSEFLG_PROFEMU;

	razer_debug("Mouse profile emulation initialized for %s\n", m->idstr);

	return 0;

err_free:
	razer_free(emu, sizeof(*emu));

	return -EINVAL;
}

void razer_mouse_exit_profile_emulation(struct razer_mouse *m)
{
	struct razer_mouse_profile_emu *emu;

	if (!(m->flags & RAZER_MOUSEFLG_PROFEMU))
		return;
	emu = m->profemu;

	m->nr_profiles = 0;
	m->get_profiles = NULL;
	m->get_active_profile = NULL;
	m->set_active_profile = NULL;

	m->profemu = NULL;
	m->flags &= ~RAZER_MOUSEFLG_PROFEMU;

	razer_free(emu, sizeof(*emu));
}
