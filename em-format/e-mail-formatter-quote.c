/*
 * e-mail-formatter-quote.c
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
 */

#include "e-mail-formatter-quote.h"

#include <camel/camel.h>

#include "e-mail-formatter-quote.h"
#include "e-mail-part.h"
#include "e-mail-part-attachment.h"
#include "e-mail-part-utils.h"

#include <libebackend/libebackend.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#define E_MAIL_FORMATTER_QUOTE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_FORMATTER_QUOTE, EMailFormatterQuotePrivate))

struct _EMailFormatterQuotePrivate {
	gchar *credits;
	EMailFormatterQuoteFlags flags;
};

/* internal formatter extensions */
GType e_mail_formatter_quote_attachment_get_type (void);
GType e_mail_formatter_quote_headers_get_type (void);
GType e_mail_formatter_quote_message_rfc822_get_type (void);
GType e_mail_formatter_quote_text_enriched_get_type (void);
GType e_mail_formatter_quote_text_html_get_type (void);
GType e_mail_formatter_quote_text_plain_get_type (void);

void e_mail_formatter_quote_internal_extensions_load (EMailExtensionRegistry *ereg);

static gpointer e_mail_formatter_quote_parent_class = 0;

static void
mail_formatter_quote_run (EMailFormatter *formatter,
                          EMailFormatterContext *context,
                          CamelStream *stream,
                          GCancellable *cancellable)
{
	EMailFormatterQuote *qf;
	EMailFormatterQuoteContext *qf_context;
	GSettings *settings;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	qf = E_MAIL_FORMATTER_QUOTE (formatter);

	qf_context = (EMailFormatterQuoteContext *) context;
	qf_context->qf_flags = qf->priv->flags;

	g_seekable_seek (
		G_SEEKABLE (stream),
		0, G_SEEK_SET, NULL, NULL);

	settings = g_settings_new ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (
		settings, "composer-top-signature"))
		camel_stream_write_string (
			stream, "<br>\n", cancellable, NULL);
	g_object_unref (settings);

	if (qf->priv->credits && *qf->priv->credits) {
		gchar *credits = g_strdup_printf ("%s<br>", qf->priv->credits);
		camel_stream_write_string (stream, credits, cancellable, NULL);
		g_free (credits);
	} else {
		camel_stream_write_string (stream, "<br>", cancellable, NULL);
	}

	if (qf->priv->flags & E_MAIL_FORMATTER_QUOTE_FLAG_CITE) {
		camel_stream_write_string (
			stream,
			"<!--+GtkHTML:<DATA class=\"ClueFlow\" "
			"key=\"orig\" value=\"1\">-->\n"
			"<blockquote type=cite>\n", cancellable, NULL);
	}

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = link->data;

		if (g_str_has_suffix (part->id, ".headers") &&
		   !(qf_context->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS)) {
			continue;
		}

		if (g_str_has_suffix (part->id, ".rfc822")) {
			gchar *end = g_strconcat (part->id, ".end", NULL);

			while (link != NULL) {
				EMailPart *p = link->data;

				if (g_strcmp0 (p->id, end) == 0)
					break;

				link = g_list_next (link);
				if (link == NULL)
					break;
			}
			g_free (end);

			continue;
		}

		if (part->is_hidden || part->is_attachment)
			continue;

		e_mail_formatter_format_as (
			formatter, context, part, stream,
			part->mime_type, cancellable);
	}

	while (!g_queue_is_empty (&queue))
		e_mail_part_unref (g_queue_pop_head (&queue));

	if (qf->priv->flags & E_MAIL_FORMATTER_QUOTE_FLAG_CITE) {
		camel_stream_write_string (
			stream, "</blockquote><!--+GtkHTML:"
			"<DATA class=\"ClueFlow\" clear=\"orig\">-->",
			cancellable, NULL);
	}
}

static void
e_mail_formatter_quote_init (EMailFormatterQuote *formatter)
{
	formatter->priv = E_MAIL_FORMATTER_QUOTE_GET_PRIVATE (formatter);
}

static void
e_mail_formatter_quote_finalize (GObject *object)
{
	/* Chain up to parent's finalize() */
	G_OBJECT_CLASS (e_mail_formatter_quote_parent_class)->finalize (object);
}

static void
e_mail_formatter_quote_base_init (EMailFormatterQuoteClass *class)
{
	/* Register internal extensions. */
	g_type_ensure (e_mail_formatter_quote_attachment_get_type ());
	g_type_ensure (e_mail_formatter_quote_headers_get_type ());
	g_type_ensure (e_mail_formatter_quote_message_rfc822_get_type ());
	g_type_ensure (e_mail_formatter_quote_text_enriched_get_type ());
	g_type_ensure (e_mail_formatter_quote_text_html_get_type ());
	g_type_ensure (e_mail_formatter_quote_text_plain_get_type ());

	e_mail_formatter_extension_registry_load (
		E_MAIL_FORMATTER_CLASS (class)->extension_registry,
		E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION);

	E_MAIL_FORMATTER_CLASS (class)->text_html_flags =
		CAMEL_MIME_FILTER_TOHTML_PRE |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
}

static void
e_mail_formatter_quote_class_init (EMailFormatterQuoteClass *class)
{
	GObjectClass *object_class;
	EMailFormatterClass *formatter_class;

	e_mail_formatter_quote_parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailFormatterQuotePrivate));

	formatter_class = E_MAIL_FORMATTER_CLASS (class);
	formatter_class->context_size = sizeof (EMailFormatterQuoteContext);
	formatter_class->run = mail_formatter_quote_run;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_mail_formatter_quote_finalize;
}

GType
e_mail_formatter_quote_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EMailFormatterClass),
			(GBaseInitFunc) e_mail_formatter_quote_base_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) e_mail_formatter_quote_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,	/* class_data */
			sizeof (EMailFormatterQuote),
			0,	/* n_preallocs */
			(GInstanceInitFunc) e_mail_formatter_quote_init,
			NULL	/* value_table */
		};

		type = g_type_register_static (
			E_TYPE_MAIL_FORMATTER,
			"EMailFormatterQuote", &type_info, 0);
	}

	return type;
}

EMailFormatter *
e_mail_formatter_quote_new (const gchar *credits,
                            EMailFormatterQuoteFlags flags)
{
	EMailFormatterQuote *formatter;
	formatter = g_object_new (E_TYPE_MAIL_FORMATTER_QUOTE, NULL);

	formatter->priv->credits = g_strdup (credits);
	formatter->priv->flags = flags;

	return (EMailFormatter *) formatter;
}

/* ------------------------------------------------------------------------- */

G_DEFINE_ABSTRACT_TYPE (
	EMailFormatterQuoteExtension,
	e_mail_formatter_quote_extension,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static void
e_mail_formatter_quote_extension_class_init (EMailFormatterQuoteExtensionClass *class)
{
}

static void
e_mail_formatter_quote_extension_init (EMailFormatterQuoteExtension *extension)
{
}

