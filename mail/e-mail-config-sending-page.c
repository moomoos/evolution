/*
 * e-mail-config-sending-page.c
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

#include "e-mail-config-sending-page.h"

#include <config.h>
#include <glib/gi18n-lib.h>

/* Forward Declarations */
static void	e_mail_config_sending_page_interface_init
					(EMailConfigPageInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EMailConfigSendingPage,
	e_mail_config_sending_page,
	E_TYPE_MAIL_CONFIG_SERVICE_PAGE,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_CONFIG_PAGE,
		e_mail_config_sending_page_interface_init))

static void
e_mail_config_sending_page_class_init (EMailConfigSendingPageClass *class)
{
	EMailConfigServicePageClass *service_page_class;

	service_page_class = E_MAIL_CONFIG_SERVICE_PAGE_CLASS (class);
	service_page_class->extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
	service_page_class->provider_type = CAMEL_PROVIDER_TRANSPORT;
	service_page_class->default_backend_name = "smtp";
}

static void
e_mail_config_sending_page_interface_init (EMailConfigPageInterface *interface)
{
	interface->title = _("Sending Email");
	interface->sort_order = E_MAIL_CONFIG_SENDING_PAGE_SORT_ORDER;
}

static void
e_mail_config_sending_page_init (EMailConfigSendingPage *page)
{
}

EMailConfigPage *
e_mail_config_sending_page_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_SENDING_PAGE,
		"registry", registry, NULL);
}

