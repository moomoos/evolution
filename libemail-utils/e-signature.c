/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <glib/gi18n-lib.h>

#include <libedataserver/e-uid.h>
#include <libedataserver/e-data-server-util.h>

#include "e-signature.h"

struct _ESignaturePrivate {
	gchar *filename;
	gchar *name;
	gchar *uid;

	gboolean autogenerated;
	gboolean is_html;
	gboolean is_script;
};

enum {
	PROP_0,
	PROP_AUTOGENERATED,
	PROP_FILENAME,
	PROP_IS_HTML,
	PROP_IS_SCRIPT,
	PROP_NAME,
	PROP_UID
};

G_DEFINE_TYPE (
	ESignature,
	e_signature,
	G_TYPE_OBJECT)

static gboolean
xml_set_bool (xmlNodePtr node,
              const gchar *name,
              gboolean *val)
{
	gboolean v_boolean;
	gchar *buf;

	if ((buf = (gchar *) xmlGetProp (node, (xmlChar *) name))) {
		v_boolean = (!strcmp (buf, "true") || !strcmp (buf, "yes"));
		xmlFree (buf);

		if (v_boolean != *val) {
			*val = v_boolean;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
xml_set_prop (xmlNodePtr node,
              const gchar *name,
              gchar **val)
{
	gchar *buf, *new_val;

	buf = (gchar *) xmlGetProp (node, (xmlChar *) name);
	new_val = g_strdup (buf);
	xmlFree (buf);

	/* We can use strcmp here whether the value is UTF8 or
	 * not, since we only care if the bytes changed.
	 */
	if (!*val || strcmp (*val, new_val)) {
		g_free (*val);
		*val = new_val;
		return TRUE;
	} else {
		g_free (new_val);
		return FALSE;
	}
}

static gboolean
xml_set_content (xmlNodePtr node,
                 gchar **val)
{
	gchar *buf, *new_val;

	buf = (gchar *) xmlNodeGetContent (node);
	new_val = g_strdup (buf);
	xmlFree (buf);

	/* We can use strcmp here whether the value is UTF8 or
	 * not, since we only care if the bytes changed. */
	if (!*val || strcmp (*val, new_val)) {
		g_free (*val);
		*val = new_val;
		return TRUE;
	} else {
		g_free (new_val);
		return FALSE;
	}
}

static void
signature_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_AUTOGENERATED:
			e_signature_set_autogenerated (
				E_SIGNATURE (object),
				g_value_get_boolean (value));
			return;

		case PROP_FILENAME:
			e_signature_set_filename (
				E_SIGNATURE (object),
				g_value_get_string (value));
			return;

		case PROP_IS_HTML:
			e_signature_set_is_html (
				E_SIGNATURE (object),
				g_value_get_boolean (value));
			return;

		case PROP_IS_SCRIPT:
			e_signature_set_is_script (
				E_SIGNATURE (object),
				g_value_get_boolean (value));
			return;

		case PROP_NAME:
			e_signature_set_name (
				E_SIGNATURE (object),
				g_value_get_string (value));
			return;

		case PROP_UID:
			e_signature_set_uid (
				E_SIGNATURE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_get_property (GObject *object,
                        guint property_id,
                        GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_AUTOGENERATED:
			g_value_set_boolean (
				value, e_signature_get_autogenerated (
				E_SIGNATURE (object)));
			return;

		case PROP_FILENAME:
			g_value_set_string (
				value, e_signature_get_filename (
				E_SIGNATURE (object)));
			return;

		case PROP_IS_HTML:
			g_value_set_boolean (
				value, e_signature_get_is_html (
				E_SIGNATURE (object)));
			return;

		case PROP_IS_SCRIPT:
			g_value_set_boolean (
				value, e_signature_get_is_script (
				E_SIGNATURE (object)));
			return;

		case PROP_NAME:
			g_value_set_string (
				value, e_signature_get_name (
				E_SIGNATURE (object)));
			return;

		case PROP_UID:
			g_value_set_string (
				value, e_signature_get_uid (
				E_SIGNATURE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_finalize (GObject *object)
{
	ESignaturePrivate *priv;

	priv = E_SIGNATURE (object)->priv;

	g_free (priv->filename);
	g_free (priv->name);
	g_free (priv->uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_signature_parent_class)->finalize (object);
}

static void
e_signature_class_init (ESignatureClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESignaturePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = signature_set_property;
	object_class->get_property = signature_get_property;
	object_class->finalize = signature_finalize;

	g_object_class_install_property (
		object_class,
		PROP_AUTOGENERATED,
		g_param_spec_boolean (
			"autogenerated",
			"Autogenerated",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			"Filename",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_IS_HTML,
		g_param_spec_boolean (
			"is-html",
			"Is HTML",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_IS_SCRIPT,
		g_param_spec_boolean (
			"is-script",
			"Is Script",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_NAME,
		g_param_spec_string (
			"name",
			"Name",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_UID,
		g_param_spec_string (
			"uid",
			"UID",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_signature_init (ESignature *signature)
{
	signature->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		signature, E_TYPE_SIGNATURE, ESignaturePrivate);
}

/**
 * e_signature_new:
 *
 * Returns a new signature which can be filled in and
 * added to an #ESignatureList.
 *
 * Returns: a new #ESignature
 **/
ESignature *
e_signature_new (void)
{
	ESignature *signature;

	signature = g_object_new (E_TYPE_SIGNATURE, NULL);
	signature->priv->uid = e_uid_new ();

	return signature;
}

/**
 * e_signature_new_from_xml:
 * @xml: an XML signature description
 *
 * Return value: a new #ESignature based on the data in @xml, or %NULL
 * if @xml could not be parsed as valid signature data.
 **/
ESignature *
e_signature_new_from_xml (const gchar *xml)
{
	ESignature *signature;

	signature = g_object_new (E_TYPE_SIGNATURE, NULL);

	if (!e_signature_set_from_xml (signature, xml)) {
		g_object_unref (signature);
		return NULL;
	}

	return signature;
}

/**
 * e_signature_uid_from_xml:
 * @xml: an XML signature description
 *
 * Return value: the permanent UID of the signature described by @xml
 * (or %NULL if @xml could not be parsed or did not contain a uid).
 * The caller must free this string.
 **/
gchar *
e_signature_uid_from_xml (const gchar *xml)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	gchar *uid = NULL;

	if (!(doc = xmlParseDoc ((xmlChar *) xml)))
		return NULL;

	node = doc->children;
	if (strcmp ((gchar *)node->name, "signature") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}

	xml_set_prop (node, "uid", &uid);
	xmlFreeDoc (doc);

	return uid;
}

/**
 * e_signature_set_from_xml:
 * @signature: an #ESignature
 * @xml: an XML signature description.
 *
 * Changes @signature to match @xml.
 *
 * Returns: %TRUE if the signature was loaded or %FALSE otherwise
 **/
gboolean
e_signature_set_from_xml (ESignature *signature,
                          const gchar *xml)
{
	gboolean changed = FALSE;
	xmlNodePtr node, cur;
	xmlDocPtr doc;
	gboolean bool;
	gchar *buf;

	if (!(doc = xmlParseDoc ((xmlChar *) xml)))
		return FALSE;

	node = doc->children;
	if (strcmp ((gchar *)node->name, "signature") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	buf = NULL;
	xml_set_prop (node, "uid", &buf);

	if (buf && *buf) {
		g_free (signature->priv->uid);
		signature->priv->uid = buf;
	}

	changed |= xml_set_prop (node, "name", &signature->priv->name);
	changed |= xml_set_bool (node, "auto", &signature->priv->autogenerated);

	if (e_signature_get_autogenerated (signature)) {
		xmlFreeDoc (doc);

		return changed;
	}

	buf = NULL;
	xml_set_prop (node, "format", &buf);
	if (buf && !strcmp (buf, "text/html"))
		bool = TRUE;
	else
		bool = FALSE;
	g_free (buf);

	if (e_signature_get_is_html (signature) != bool) {
		e_signature_set_is_html (signature, bool);
		changed = TRUE;
	}

	cur = node->children;
	while (cur) {
		if (!strcmp ((gchar *)cur->name, "filename")) {
			changed |= xml_set_content (
				cur, &signature->priv->filename);
			changed |= xml_set_bool (
				cur, "script", &signature->priv->is_script);
			break;
		} else if (!strcmp ((gchar *)cur->name, "script")) {
			/* this is for handling 1.4 signature script definitions */
			changed |= xml_set_content (
				cur, &signature->priv->filename);
			if (!e_signature_get_is_script (signature)) {
				e_signature_set_is_script (signature, TRUE);
				changed = TRUE;
			}
			break;
		}
		cur = cur->next;
	}

	/* If the signature is not a script, replace the directory
	 * part with the current signatures directory.  This makes
	 * moving the signatures directory transparent. */
	if (!e_signature_get_is_script (signature)) {
		const gchar *user_data_dir;
		gchar *basename;
		gchar *filename;

		user_data_dir = e_get_user_data_dir ();

		filename = signature->priv->filename;
		basename = g_path_get_basename (filename);
		signature->priv->filename = g_build_filename (
			user_data_dir, "signatures", basename, NULL);
		g_free (basename);
		g_free (filename);
	}

	xmlFreeDoc (doc);

	return changed;
}

/**
 * e_signature_to_xml:
 * @signature: an #ESignature
 *
 * Return value: an XML representation of @signature, which the caller
 * must free.
 **/
gchar *
e_signature_to_xml (ESignature *signature)
{
	xmlChar *xmlbuf;
	gchar *tmp;
	xmlNodePtr root, node;
	xmlDocPtr doc;
	const gchar *string;
	gint n;

	doc = xmlNewDoc ((xmlChar *) "1.0");

	root = xmlNewDocNode (doc, NULL, (xmlChar *) "signature", NULL);
	xmlDocSetRootElement (doc, root);

	string = e_signature_get_name (signature);
	xmlSetProp (root, (xmlChar *) "name", (xmlChar *) string);

	string = e_signature_get_uid (signature);
	xmlSetProp (root, (xmlChar *) "uid", (xmlChar *) string);

	if (e_signature_get_autogenerated (signature))
		string = "true";
	else
		string = "false";
	xmlSetProp (root, (xmlChar *) "auto", (xmlChar *) string);

	if (!e_signature_get_autogenerated (signature)) {
		if (e_signature_get_is_html (signature))
			string = "text/html";
		else
			string = "text/plain";
		xmlSetProp (root, (xmlChar *) "format", (xmlChar *) string);

		string = e_signature_get_filename (signature);
		if (string != NULL) {

			/* For scripts we save the full filename,
			 * for normal signatures just the basename. */
			if (e_signature_get_is_script (signature)) {
				node = xmlNewTextChild (
					root, NULL, (xmlChar *) "filename",
					(xmlChar *) string);
				xmlSetProp (
					node, (xmlChar *) "script",
					(xmlChar *) "true");
			} else {
				gchar *basename;

				basename = g_path_get_basename (string);
				node = xmlNewTextChild (
					root, NULL, (xmlChar *) "filename",
					(xmlChar *) basename);
				g_free (basename);
			}
		}
	} else {
		/* this is to make Evolution-1.4 and older 1.5 versions happy */
		xmlSetProp (root, (xmlChar *) "format", (xmlChar *) "text/html");
	}

	xmlDocDumpMemory (doc, &xmlbuf, &n);
	xmlFreeDoc (doc);

	/* remap to glib memory */
	tmp = g_malloc (n + 1);
	memcpy (tmp, xmlbuf, n);
	tmp[n] = '\0';
	xmlFree (xmlbuf);

	return tmp;
}

gboolean
e_signature_is_equal (ESignature *signature1,
                      ESignature *signature2)
{
	const gchar *uid1;
	const gchar *uid2;

	g_return_val_if_fail (E_IS_SIGNATURE (signature1), FALSE);
	g_return_val_if_fail (E_IS_SIGNATURE (signature2), FALSE);

	/* XXX Simply compares the UIDs.  Not fool-proof. */
	uid1 = e_signature_get_uid (signature1);
	uid2 = e_signature_get_uid (signature2);

	return (g_strcmp0 (uid1, uid2) == 0);
}

gboolean
e_signature_get_autogenerated (ESignature *signature)
{
	g_return_val_if_fail (E_IS_SIGNATURE (signature), FALSE);

	return signature->priv->autogenerated;
}

void
e_signature_set_autogenerated (ESignature *signature,
                               gboolean autogenerated)
{
	g_return_if_fail (E_IS_SIGNATURE (signature));

	if (signature->priv->autogenerated == autogenerated)
		return;

	signature->priv->autogenerated = autogenerated;

	/* Autogenerated flags overrides several properties. */
	g_object_freeze_notify (G_OBJECT (signature));
	g_object_notify (G_OBJECT (signature), "autogenerated");
	g_object_notify (G_OBJECT (signature), "filename");
	g_object_notify (G_OBJECT (signature), "is-html");
	g_object_notify (G_OBJECT (signature), "is-script");
	g_object_notify (G_OBJECT (signature), "name");
	g_object_thaw_notify (G_OBJECT (signature));
}

const gchar *
e_signature_get_filename (ESignature *signature)
{
	g_return_val_if_fail (E_IS_SIGNATURE (signature), NULL);

	/* Autogenerated flags overrides the filename property. */
	if (e_signature_get_autogenerated (signature))
		return NULL;

	return signature->priv->filename;
}

void
e_signature_set_filename (ESignature *signature,
                          const gchar *filename)
{
	g_return_if_fail (E_IS_SIGNATURE (signature));

	g_free (signature->priv->filename);
	signature->priv->filename = g_strdup (filename);

	g_object_notify (G_OBJECT (signature), "filename");
}

gboolean
e_signature_get_is_html (ESignature *signature)
{
	g_return_val_if_fail (E_IS_SIGNATURE (signature), FALSE);

	/* Autogenerated flag overrides the is-html property. */
	if (e_signature_get_autogenerated (signature))
		return FALSE;

	return signature->priv->is_html;
}

void
e_signature_set_is_html (ESignature *signature,
                         gboolean is_html)
{
	g_return_if_fail (E_IS_SIGNATURE (signature));

	if (signature->priv->is_html == is_html)
		return;

	signature->priv->is_html = is_html;

	g_object_notify (G_OBJECT (signature), "is-html");
}

gboolean
e_signature_get_is_script (ESignature *signature)
{
	g_return_val_if_fail (E_IS_SIGNATURE (signature), FALSE);

	/* Autogenerated flags overrides the is-script property. */
	if (e_signature_get_autogenerated (signature))
		return FALSE;

	return signature->priv->is_script;
}

void
e_signature_set_is_script (ESignature *signature,
                           gboolean is_script)
{
	g_return_if_fail (E_IS_SIGNATURE (signature));

	if (signature->priv->is_script == is_script)
		return;

	signature->priv->is_script = is_script;

	g_object_notify (G_OBJECT (signature), "is-script");
}

const gchar *
e_signature_get_name (ESignature *signature)
{
	g_return_val_if_fail (E_IS_SIGNATURE (signature), NULL);

	/* Autogenerated flag overrides the name property. */
	if (e_signature_get_autogenerated (signature))
		return _("Autogenerated");

	return signature->priv->name;
}

void
e_signature_set_name (ESignature *signature,
                      const gchar *name)
{
	g_return_if_fail (E_IS_SIGNATURE (signature));

	g_free (signature->priv->name);
	signature->priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (signature), "name");
}

const gchar *
e_signature_get_uid (ESignature *signature)
{
	g_return_val_if_fail (E_IS_SIGNATURE (signature), NULL);

	return signature->priv->uid;
}

void
e_signature_set_uid (ESignature *signature,
                     const gchar *uid)
{
	g_return_if_fail (E_IS_SIGNATURE (signature));

	g_free (signature->priv->uid);

	if (uid == NULL)
		signature->priv->uid = e_uid_new ();
	else
		signature->priv->uid = g_strdup (uid);

	g_object_notify (G_OBJECT (signature), "uid");
}
