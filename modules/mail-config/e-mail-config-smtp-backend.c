/*
 * e-mail-config-smtp-backend.c
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

#include "e-mail-config-smtp-backend.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <libebackend/libebackend.h>

#include <misc/e-port-entry.h>

#include <mail/e-mail-config-auth-check.h>
#include <mail/e-mail-config-service-page.h>

#define E_MAIL_CONFIG_SMTP_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_SMTP_BACKEND, EMailConfigSmtpBackendPrivate))

struct _EMailConfigSmtpBackendPrivate {
	GtkWidget *host_entry;			/* not referenced */
	GtkWidget *port_entry;			/* not referenced */
	GtkWidget *user_entry;			/* not referenced */
	GtkWidget *security_combo_box;		/* not referenced */
	GtkWidget *auth_required_toggle;	/* not referenced */
	GtkWidget *auth_check;			/* not referenced */
};

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigSmtpBackend,
	e_mail_config_smtp_backend,
	E_TYPE_MAIL_CONFIG_SERVICE_BACKEND)

static void
mail_config_smtp_backend_insert_widgets (EMailConfigServiceBackend *backend,
                                         GtkBox *parent)
{
	EMailConfigSmtpBackendPrivate *priv;
	CamelProvider *provider;
	CamelSettings *settings;
	ESource *source;
	ESourceBackend *extension;
	EMailConfigServicePage *page;
	EMailConfigServicePageClass *class;
	GtkLabel *label;
	GtkWidget *widget;
	GtkWidget *container;
	const gchar *backend_name;
	const gchar *extension_name;
	const gchar *mechanism;
	const gchar *text;
	gchar *markup;

	priv = E_MAIL_CONFIG_SMTP_BACKEND_GET_PRIVATE (backend);

	page = e_mail_config_service_backend_get_page (backend);
	source = e_mail_config_service_backend_get_source (backend);
	settings = e_mail_config_service_backend_get_settings (backend);

	class = E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS (page);
	extension_name = class->extension_name;
	extension = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (extension);

	text = _("Configuration");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_grid_new ();
	gtk_widget_set_margin_left (widget, 12);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("_Server:"));
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	priv->host_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Port:"));
	gtk_grid_attach (GTK_GRID (container), widget, 2, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_port_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 3, 0, 1, 1);
	priv->port_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	text = _("Ser_ver requires authentication");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 3, 1);
	priv->auth_required_toggle = widget;  /* do not reference */
	gtk_widget_show (widget);

	text = _("Security");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_widget_set_margin_top (widget, 6);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_grid_new ();
	gtk_widget_set_margin_left (widget, 12);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Encryption _method:"));
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	/* The IDs correspond to the CamelNetworkSecurityMethod enum. */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"none",
		_("No encryption"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"starttls-on-standard-port",
		_("STARTTLS after connecting"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"ssl-on-alternate-port",
		_("SSL on a dedicated port"));
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	priv->security_combo_box = widget;  /* do not reference */
	gtk_widget_show (widget);

	provider = camel_provider_get (backend_name, NULL);
	if (provider != NULL && provider->port_entries != NULL)
		e_port_entry_set_camel_entries (
			E_PORT_ENTRY (priv->port_entry),
			provider->port_entries);

	text = _("Authentication");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_widget_set_margin_top (widget, 6);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	g_object_bind_property (
		priv->auth_required_toggle, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	widget = gtk_grid_new ();
	gtk_widget_set_margin_left (widget, 12);
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_object_bind_property (
		priv->auth_required_toggle, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("T_ype:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	/* We can't bind GSettings:auth-mechanism directly to this
	 * because the toggle button above influences the value we
	 * choose: "none" if the toggle button is inactive or else
	 * the active mechanism name from this widget. */
	widget = e_mail_config_auth_check_new (backend);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	priv->auth_check = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("User_name:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 2, 1);
	priv->user_entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	g_object_bind_property (
		settings, "host",
		priv->host_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_bind_property_full (
		settings, "security-method",
		priv->security_combo_box, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	g_object_bind_property (
		settings, "port",
		priv->port_entry, "port",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		settings, "security-method",
		priv->port_entry, "security-method",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		settings, "user",
		priv->user_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Enable the auth-required toggle button if
	 * we have an authentication mechanism name. */
	mechanism = camel_network_settings_get_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (priv->auth_required_toggle),
		(mechanism != NULL && *mechanism != '\0'));
}

static gboolean
mail_config_smtp_backend_auto_configure (EMailConfigServiceBackend *backend,
                                         EMailAutoconfig *autoconfig)
{
	ESource *source;

	source = e_mail_config_service_backend_get_source (backend);

	return e_mail_autoconfig_set_smtp_details (autoconfig, source);
}

static gboolean
mail_config_smtp_backend_check_complete (EMailConfigServiceBackend *backend)
{
	EMailConfigSmtpBackendPrivate *priv;
	CamelSettings *settings;
	CamelNetworkSettings *network_settings;
	GtkToggleButton *toggle_button;
	EPortEntry *port_entry;
	const gchar *host;
	const gchar *user;

	priv = E_MAIL_CONFIG_SMTP_BACKEND_GET_PRIVATE (backend);

	settings = e_mail_config_service_backend_get_settings (backend);

	network_settings = CAMEL_NETWORK_SETTINGS (settings);
	host = camel_network_settings_get_host (network_settings);
	user = camel_network_settings_get_user (network_settings);

	if (host == NULL || *host == '\0')
		return FALSE;

	port_entry = E_PORT_ENTRY (priv->port_entry);

	if (!e_port_entry_is_valid (port_entry))
		return FALSE;

	toggle_button = GTK_TOGGLE_BUTTON (priv->auth_required_toggle);

	if (gtk_toggle_button_get_active (toggle_button))
		if (user == NULL || *user == '\0')
			return FALSE;

	return TRUE;
}

static void
mail_config_smtp_backend_commit_changes (EMailConfigServiceBackend *backend)
{
	EMailConfigSmtpBackendPrivate *priv;
	GtkToggleButton *toggle_button;
	CamelSettings *settings;
	const gchar *mechanism = NULL;

	/* The authentication mechanism name depends on both the
	 * toggle button and the EMailConfigAuthCheck widget, so
	 * we have to set it manually here. */

	priv = E_MAIL_CONFIG_SMTP_BACKEND_GET_PRIVATE (backend);

	settings = e_mail_config_service_backend_get_settings (backend);

	toggle_button = GTK_TOGGLE_BUTTON (priv->auth_required_toggle);

	if (gtk_toggle_button_get_active (toggle_button))
		mechanism = e_mail_config_auth_check_get_active_mechanism (
			E_MAIL_CONFIG_AUTH_CHECK (priv->auth_check));

	camel_network_settings_set_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings), mechanism);
}

static void
e_mail_config_smtp_backend_class_init (EMailConfigSmtpBackendClass *class)
{
	EMailConfigServiceBackendClass *backend_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigSmtpBackendPrivate));

	backend_class = E_MAIL_CONFIG_SERVICE_BACKEND_CLASS (class);
	backend_class->backend_name = "smtp";
	backend_class->insert_widgets = mail_config_smtp_backend_insert_widgets;
	backend_class->auto_configure = mail_config_smtp_backend_auto_configure;
	backend_class->check_complete = mail_config_smtp_backend_check_complete;
	backend_class->commit_changes = mail_config_smtp_backend_commit_changes;
}

static void
e_mail_config_smtp_backend_class_finalize (EMailConfigSmtpBackendClass *class)
{
}

static void
e_mail_config_smtp_backend_init (EMailConfigSmtpBackend *backend)
{
	backend->priv = E_MAIL_CONFIG_SMTP_BACKEND_GET_PRIVATE (backend);
}

void
e_mail_config_smtp_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_smtp_backend_register_type (type_module);
}

