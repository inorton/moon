/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * resources.cpp
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#include <config.h>
#include <stdlib.h>

#include "runtime.h"
#include "resources.h"
#include "namescope.h"
#include "error.h"


//
// ResourceDictionaryIterator
//

ResourceDictionaryIterator::ResourceDictionaryIterator (ResourceDictionary *resources) : CollectionIterator (resources)
{
	Init ();
}

void
ResourceDictionaryIterator::Init ()
{
	g_hash_table_iter_init (&iter, ((ResourceDictionary *) collection)->hash);
	value = NULL;
	key = NULL;
}

bool
ResourceDictionaryIterator::Next (CollectionIteratorError *err)
{
	if (generation != collection->Generation ()) {
		*err = CollectionIteratorErrorMutated;
		return false;
	}
	
	*err = CollectionIteratorErrorNone;
	
	if (!g_hash_table_iter_next (&iter, &key, &value)) {
		key = value = NULL;
		return false;
	}
	
	return true;
}

bool
ResourceDictionaryIterator::Reset ()
{
	if (generation != collection->Generation ())
		return false;
	
	Init ();
	
	return true;
}

Value *
ResourceDictionaryIterator::GetCurrent (CollectionIteratorError *err)
{
	if (generation != collection->Generation ()) {
		*err = CollectionIteratorErrorMutated;
		return NULL;
	}
	
	if (key == NULL) {
		*err = CollectionIteratorErrorBounds;
		return NULL;
	}
	
	*err = CollectionIteratorErrorNone;
	
	return (Value *) value;
}

const char *
ResourceDictionaryIterator::GetCurrentKey (CollectionIteratorError *err)
{
	if (generation != collection->Generation ()) {
		*err = CollectionIteratorErrorMutated;
		return NULL;
	}
	
	if (key == NULL) {
		*err = CollectionIteratorErrorBounds;
		return NULL;
	}
	
	*err = CollectionIteratorErrorNone;
	
	return (const char *) key;
}


//
// ResourceDictionary
//

static void
free_value (Value *value)
{
	delete value;
}

ResourceDictionary::ResourceDictionary ()
{
	SetObjectType (Type::RESOURCE_DICTIONARY);
	hash = g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      (GDestroyNotify)g_free,
				      (GDestroyNotify)free_value);
	from_resource_dictionary_api = false;
}

ResourceDictionary::~ResourceDictionary ()
{
	g_hash_table_destroy (hash);
}

CollectionIterator *
ResourceDictionary::GetIterator ()
{
	return new ResourceDictionaryIterator (this);
}

bool
ResourceDictionary::CanAdd (Value *value)
{
	return true;
}

bool
ResourceDictionary::Add (const char* key, Value *value)
{
	MoonError err;
	return AddWithError (key, value, &err);
}

bool
ResourceDictionary::AddWithError (const char* key, Value *value, MoonError *error)
{
	if (!key) {
		MoonError::FillIn (error, MoonError::ARGUMENT_NULL, "key was null");
		return false;
	}

	if (ContainsKey (key)) {
		MoonError::FillIn (error, MoonError::ARGUMENT, "An item with the same key has already been added");
		return false;
	}

	Value *v = new Value (*value);
	
	from_resource_dictionary_api = true;
	bool result = Collection::AddWithError (v, error) != -1;
	from_resource_dictionary_api = false;
	if (result)
		g_hash_table_insert (hash, g_strdup (key), v);
	return result;
}

bool
ResourceDictionary::Clear ()
{
#if GLIB_CHECK_VERSION(2,12,0)
	if (glib_check_version (2,12,0))
		g_hash_table_remove_all (hash);
	else
#endif
	g_hash_table_foreach_remove (hash, (GHRFunc) gtk_true, NULL);

	from_resource_dictionary_api = true;
	bool rv = Collection::Clear ();
	from_resource_dictionary_api = false;

	return rv;
}

bool
ResourceDictionary::ContainsKey (const char *key)
{
	if (!key)
		return false;

	gpointer orig_value;
	gpointer orig_key;

	return g_hash_table_lookup_extended (hash, key,
					     &orig_key, &orig_value);
}

bool
ResourceDictionary::Remove (const char *key)
{
	if (!key)
		return false;

	/* check if the item exists first */
	Value* orig_value;
	gpointer orig_key;

	if (!g_hash_table_lookup_extended (hash, key,
					   &orig_key, (gpointer*)&orig_value))
		return false;

	from_resource_dictionary_api = true;
	Collection::Remove (orig_value);
	from_resource_dictionary_api = false;

	g_hash_table_remove (hash, key);

	return true;
}

bool
ResourceDictionary::Set (const char *key, Value *value)
{
	/* check if the item exists first */
	Value* orig_value;
	gpointer orig_key;

	if (g_hash_table_lookup_extended (hash, key,
					  &orig_key, (gpointer*)&orig_value)) {
		return false;
	}

	Value *v = new Value (*value);

	from_resource_dictionary_api = true;
	Collection::Remove (orig_value);
	Collection::Add (v);
	from_resource_dictionary_api = false;

	g_hash_table_replace (hash, g_strdup (key), v);

	return true; // XXX
}

Value*
ResourceDictionary::Get (const char *key, bool *exists)
{
	Value *v = NULL;
	gpointer orig_key;

	*exists = g_hash_table_lookup_extended (hash, key,
						&orig_key, (gpointer*)&v);

	if (!*exists)
		v = GetFromMergedDictionaries (key, exists);

	return v;
}

Value *
ResourceDictionary::GetFromMergedDictionaries (const char *key, bool *exists)
{
	Value *v = NULL;

	ResourceDictionaryCollection *merged = GetMergedDictionaries ();

	if (!merged) {
		*exists = false;
		return NULL;
	}

	CollectionIterator *iter = merged->GetIterator ();
	CollectionIteratorError err;
	
	while (iter->Next (&err) && !*exists) {
		Value *dict_v = iter->GetCurrent (&err);

		if (err)
			break;
		
		ResourceDictionary *dict = dict_v->AsResourceDictionary ();
		v = dict->Get (key, exists);
	}

	return v;
}

static bool
can_be_added_twice (Deployment *deployment, Value *value)
{
	static Type::Kind twice_kinds [] = {
		Type::FRAMEWORKTEMPLATE,
		Type::STYLE,
		Type::STROKE_COLLECTION,
		Type::DRAWINGATTRIBUTES,
		Type::TRANSFORM,
		Type::BRUSH,
		Type::STYLUSPOINT_COLLECTION,
		Type::BITMAPIMAGE,
		Type::STROKE,
		Type::INVALID
	};

	for (int i = 0; twice_kinds [i] != Type::INVALID; i++) {
		if (Type::IsSubclassOf (deployment, value->GetKind (), twice_kinds [i]))
			return true;
	}

	return false;
}

// XXX this was (mostly, except for the type check) c&p from DependencyObjectCollection
bool
ResourceDictionary::AddedToCollection (Value *value, MoonError *error)
{
	DependencyObject *obj = NULL;
	bool rv = false;
	
	if (value->Is(GetDeployment (), Type::DEPENDENCY_OBJECT)) {
		obj = value->AsDependencyObject ();
		DependencyObject *parent = obj ? obj->GetParent () : NULL;
		// Call SetSurface() /before/ setting the logical parent
		// because Storyboard::SetSurface() needs to be able to
		// distinguish between the two cases.
	
		if (parent && !can_be_added_twice (GetDeployment (), value)) {
			MoonError::FillIn (error, MoonError::INVALID_OPERATION, g_strdup_printf ("Element is already a child of another element.  %s", Type::Find (GetDeployment (), value->GetKind ())->GetName ()));
			return false;
		}
		
		obj->SetIsAttached (IsAttached ());
		obj->SetParent (this, error);
		if (error->number)
			return false;

		obj->AddPropertyChangeListener (this);

		if (!from_resource_dictionary_api) {
			const char *key = obj->GetName();

			if (!key) {
				MoonError::FillIn (error, MoonError::ARGUMENT_NULL, "key was null");
				goto cleanup;
			}

			if (ContainsKey (key)) {
				MoonError::FillIn (error, MoonError::ARGUMENT, "An item with the same key has already been added");
				goto cleanup;
			}
		}
	}

	rv = Collection::AddedToCollection (value, error);

	if (rv && !from_resource_dictionary_api && obj != NULL) {
		const char *key = obj->GetName();

		g_hash_table_insert (hash, g_strdup (key), new Value (obj));
	}

cleanup:
	if (!rv) {
		if (obj) {
			/* If we set the parent, but the object wasn't added to the collection, make sure we clear the parent */
			printf ("ResourceDictionary::AddedToCollection (): not added, clearing parent from %p\n", obj);
			obj->SetParent (NULL, NULL);
		}
	}

	return rv;
}

static gboolean
remove_from_hash_by_value (gpointer  key,
			   gpointer  value,
			   gpointer  user_data)
{
	Value *v = (Value*)value;
	DependencyObject *obj = (DependencyObject *) user_data;
	return (v->GetKind () == obj->GetObjectType () && v->AsDependencyObject() == obj);
}

// XXX this was (mostly, except for the type check) c&p from DependencyObjectCollection
void
ResourceDictionary::RemovedFromCollection (Value *value)
{
	if (value->Is (GetDeployment (), Type::DEPENDENCY_OBJECT)) {
		DependencyObject *obj = value->AsDependencyObject ();
		
		obj->RemovePropertyChangeListener (this);
		obj->SetParent (NULL, NULL);
		obj->SetIsAttached (false);
		
		Collection::RemovedFromCollection (value);

		if (!from_resource_dictionary_api)
			g_hash_table_foreach_remove (hash, remove_from_hash_by_value, value->AsDependencyObject ());
	}
}

// XXX this was (mostly, except for the type check) c&p from DependencyObjectCollection
void
ResourceDictionary::SetIsAttached (bool attached)
{
	if (IsAttached () == attached)
		return;

	Value *value;
	
	for (guint i = 0; i < array->len; i++) {
		value = (Value *) array->pdata[i];
		if (value->Is (GetDeployment (), Type::DEPENDENCY_OBJECT)) {
			DependencyObject *obj = value->AsDependencyObject ();
			obj->SetIsAttached (attached);
		}
	}
	
	Collection::SetIsAttached (attached);
}

// XXX this was (mostly, except for the type check) c&p from DependencyObjectCollection
void
ResourceDictionary::UnregisterAllNamesRootedAt (NameScope *from_ns)
{
	Value *value;
	
	for (guint i = 0; i < array->len; i++) {
		value = (Value *) array->pdata[i];
		if (value->Is (GetDeployment (), Type::DEPENDENCY_OBJECT)) {
			DependencyObject *obj = value->AsDependencyObject ();
			obj->UnregisterAllNamesRootedAt (from_ns);
		}
	}
	
	Collection::UnregisterAllNamesRootedAt (from_ns);
}

// XXX this was (mostly, except for the type check) c&p from DependencyObjectCollection
void
ResourceDictionary::RegisterAllNamesRootedAt (NameScope *to_ns, MoonError *error)
{
	Value *value;
	
	for (guint i = 0; i < array->len; i++) {
		if (error->number)
			break;

		value = (Value *) array->pdata[i];
		if (value->Is (GetDeployment (), Type::DEPENDENCY_OBJECT)) {
			DependencyObject *obj = value->AsDependencyObject ();
			obj->RegisterAllNamesRootedAt (to_ns, error);
		}
	}
	
	Collection::RegisterAllNamesRootedAt (to_ns, error);
}
