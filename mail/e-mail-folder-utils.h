/*
 * e-mail-folder-utils.h
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

#ifndef E_MAIL_FOLDER_UTILS_H
#define E_MAIL_FOLDER_UTILS_H

/* CamelFolder wrappers with Evolution-specific policies. */

#include <camel/camel.h>

G_BEGIN_DECLS

gboolean	e_mail_folder_append_message_sync
						(CamelFolder *folder,
						 CamelMimeMessage *message,
						 CamelMessageInfo *info,
						 gchar **appended_uid,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_folder_append_message	(CamelFolder *folder,
						 CamelMimeMessage *message,
						 CamelMessageInfo *info,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_folder_append_message_finish
						(CamelFolder *folder,
						 GAsyncResult *result,
						 gchar **appended_uid,
						 GError **error);

CamelMimePart *	e_mail_folder_build_attachment_sync
						(CamelFolder *folder,
						 GPtrArray *message_uids,
						 gchar **fwd_subject,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_folder_build_attachment	(CamelFolder *folder,
						 GPtrArray *message_uids,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelMimePart *	e_mail_folder_build_attachment_finish
						(CamelFolder *folder,
						 GAsyncResult *result,
						 gchar **fwd_subject,
						 GError **error);

GHashTable *	e_mail_folder_find_duplicate_messages_sync
						(CamelFolder *folder,
						 GPtrArray *message_uids,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_folder_find_duplicate_messages
						(CamelFolder *folder,
						 GPtrArray *message_uids,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GHashTable *	e_mail_folder_find_duplicate_messages_finish
						(CamelFolder *folder,
						 GAsyncResult *result,
						 GError **error);

GHashTable *	e_mail_folder_get_multiple_messages_sync
						(CamelFolder *folder,
						 GPtrArray *message_uids,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_folder_get_multiple_messages
						(CamelFolder *folder,
						 GPtrArray *message_uids,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GHashTable *	e_mail_folder_get_multiple_messages_finish
						(CamelFolder *folder,
						 GAsyncResult *result,
						 GError **error);

gboolean	e_mail_folder_remove_attachments_sync
						(CamelFolder *folder,
						 GPtrArray *message_uids,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_folder_remove_attachments
						(CamelFolder *folder,
						 GPtrArray *message_uids,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_folder_remove_attachments_finish
						(CamelFolder *folder,
						 GAsyncResult *result,
						 GError **error);

gchar *		e_mail_folder_uri_build		(CamelStore *store,
						 const gchar *folder_name);
gboolean	e_mail_folder_uri_parse		(CamelSession *session,
						 const gchar *folder_uri,
						 CamelStore **out_store,
						 gchar **out_folder_name,
						 GError **error);
gboolean	e_mail_folder_uri_equal		(CamelSession *session,
						 const gchar *folder_uri_a,
						 const gchar *folder_uri_b);
gchar *		e_mail_folder_uri_from_folder	(CamelFolder *folder);

G_END_DECLS

#endif /* E_MAIL_FOLDER_UTILS_H */
