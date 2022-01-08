<?php

/**
 * @file
 * Provides tokens for entity properties which have no token yet.
 */

/**
 * Implements hook_token_info_alter().
 * 
 * Since Backdrop's core already provides all token types for core entities
 * and all tokens for fields, we are very selective with the list
 * of tokens we provide here. We provide tokens not included in core.
 */
function entity_token_token_info_alter(&$info) {

  // Provide D7 Entity Tokens legacy tokens not provided by Backdrop core.
  // Tokens for type node.
  $info['tokens']['node']['is_new'] = array(
    'name' => 'Is new',
    'description' => t('Whether the node is new and not saved to the database yet.'),
    'type' => 'boolean'
  );
  $info['tokens']['node']['status'] = array(
    'name' => 'Status',
    'description' => t('Whether the node is published or unpublished.'),
  );
  $info['tokens']['node']['promoted'] = array(
    'name' => 'Promoted to frontpage',
    'description' => t('Whether the node is promoted to the frontpage.'),
  );
  $info['tokens']['node']['sticky'] = array(
    'name' => 'Sticky in lists',
    'description' => t('Whether the node is displayed at the top of lists in which it appears.'),
  );
  $info['tokens']['node']['revision'] = array(
    'name' => 'Creates revision',
    'description' => t('Whether saving this node creates a new revision.'),
  );

  $info['tokens']['comment']['subject'] = array(
    'name' => 'Subject',
    'description' => t('The subject of the comment.'),
  );
  $info['tokens']['comment']['status'] = array(
    'name' => 'Status',
    'description' => t('Whether the comment is published or unpublished.'),
  );
  $info['tokens']['comment']['comment_body'] = array(
    'name' => 'Comment',
    'description' => t('Field "comment_body". The following properties may be appended to the token: value (Text), format (Text format)'),
  );

  // Tokens for type user.
  $info['tokens']['user']['status'] = array(
    'name' => 'Status',
    'description' => t('Whether the user is active or blocked.'),
  );
  $info['tokens']['user']['last_access'] = array(
    'name' => 'Last access',
    'description' => t('The date the user last accessed the site.'),
    'type' => 'date',
  );
  $info['tokens']['user']['theme'] = array(
    'name' => 'Default theme',
    'description' => t('The user\'s default theme.'),
  );

  $info['tokens']['current-user']['status'] = array(
    'name' => 'Status',
    'description' => t('Whether the user is active or blocked.'),
  );

  // Tokens for type site
  $info['tokens']['site']['current_user'] = array(
    'name' => 'Logged in user',
    'description' => t('The currently logged in user.'),
    'type' => 'user',
  );
  $info['tokens']['site']['current_page'] = array(
    'name' => 'Current page',
    'description' => t('Information related to the current page request. The following properties may be appended to the token: path (Path), url (URL)'),
    'type' => 'struct',
  );

  // Tokens for type term.
  $info['tokens']['term']['parents_all'] = array(
    'name' => 'All parent terms',
    'description' => t('Ancestors of the term, i.e. parent of all above hierarchy levels.'),
    'type' => 'list<term>',
  );
  $info['tokens']['term']['weight'] = array(
    'name' => 'Weight',
    'description' => t('The weight of the term, which is used for ordering terms during display.'),
  );

  // Add types and tokens for all properties of entities that don't have one.
  $entity_info = entity_get_info();
  $field_info_map = field_info_field_map();
  foreach ($entity_info as $entity_name => $entity_info_array) {
    $token_type_for_entity = isset($entity_info_array['token type']) ? $entity_info_array['token type'] : $entity_name;

    // Create types for all non-core entities
    if (!_entity_token_is_core_entity($entity_name)) {
      $info['types'][$token_type_for_entity] = array(
        'name' => $entity_info[$entity_name]['label'],
        'description' => t('Tokens related to the "@name" entities.', array('@name' => $entity_info[$entity_name]['label'])),
        'needs-data' => $token_type_for_entity,
      );
    }

    // Create token definitions for each property.
    foreach (entity_plus_get_all_property_info($entity_name) as $name => $property) {
      // Skip touching the definitions for image and date fields, 
      // as these are created in core.
      if (!empty($property['field']) && in_array($field_info_map[$name]['type'], array('date', 'datetime', 'datestamp', 'image',))) {
        continue;
      }

      // Only create a token if it doesn't already exist.
      $name_with_dashes = str_replace('_', '-', $name);
      if (!isset($info['tokens'][$token_type_for_entity][$name]) && !isset($info['tokens'][$token_type_for_entity][$name_with_dashes])) {
        $info['tokens'][$token_type_for_entity][$name] = array(
          'name' => $property['label'],
          'description' => t($property['description']),
          'type' => entity_token_determine_type($property),
        );
      }
      
      // Add explanatory description for multi-value fields.
      if (!empty($property['field']) && entity_plus_property_list_extract_type($property['type'])) {
        $info['tokens'][$token_type_for_entity][$name]['dynamic'] = TRUE;
        $info['tokens'][$token_type_for_entity][$name]['description'] .= ' ' . t('Replace the ? with the delta for multi-value fields.');
      }

      // If field provides a structure (e.g. formatted text fields)
      // make the token type a struct, and add the keys to
      // the token description.
      if (isset($property['property info'])) {
        $info['tokens'][$token_type_for_entity][$name]['type'] = 'struct';
        $help = array();
        foreach ($property['property info'] as $key => $property_info) {
          $help[] = $key . ' (' . $property_info['label'] . ')';
        }
        $info['tokens'][$token_type_for_entity][$name]['description'] .= ' ' . t('The following properties may be appended to the token: @keys',
          array('@keys' => implode(', ', $help))
        );
      }
    }
  }
}

/** 
 * Implements hook_tokens().
 * This function processes:
 *  - A few unimplemented simple tokens for various entities (legacy from D7 entity_token).
 *  - Field tokens that contain chains (fields without chains are fulfilled by field_tokens()), 
 *    except chained image and date fields that are processed by core.
 *  - Fields with chains that include a delta value.
 *  - Tokens types such as list<something>.
 */
function entity_token_tokens($type, $tokens, array $data = array(), array $options = array()) {
  $replacements = array();
  $field_info_map = field_info_field_map();

  // Process tokens when passing data for an entity
  if (isset($data[$type]) && !empty($data[$type]) && $type !== 'wrapped-field' && is_a($data[$type], 'Entity')) {
    $wrapper = entity_metadata_wrapper($type, $data[$type]);
    $entity_properties = $wrapper->getPropertyInfo();
    $field_name_list = array();
    $property_name_list = array();

    foreach ($tokens as $name => $original) {
      // Fulfill simple, unchained, D7 legacy tokens for core entities, and unchained tokens for custom entities.
      if ((_entity_token_is_core_entity($type) && in_array($name, entity_token_legacy_properties())) || (!_entity_token_is_core_entity($type) && !strpos($name, ':'))) {
        // Skip if this is an unchained field - those are fulfilled by core.
        if(!empty($field_info_map[$name])) {
          continue;
        }
        try {
          if (isset($wrapper->$name)) {
            $replacement = _entity_token_get_token($wrapper->$name, $options);
            if (isset($replacement)) {
              $replacements[$original] = $replacement;
            }
          }
        }
        catch (EntityMetadataWrapperException $e) {
          // If getting values generate an error, do nothing. This may happen if the property info is not defined
        }
      } 
      // Collect field names for all chained field tokens. Non-chained field tokens are handled in core.
      elseif (strpos($name, ':') !== FALSE) {
        $name_parts = explode(':', $name);
        // Check if this is a field
        if (!empty($name_parts[0]) && isset($wrapper->{$name_parts[0]}) ) {
          $field_name = $name_parts[0];

          // Only add fields to the list. Do not add image or date fields as those are fulfilled by core.
          if (!empty($field_info_map[$field_name])) {
            if (strpos($field_info_map[$field_name]['type'], 'date') === FALSE || strpos($field_info_map[$field_name]['type'], 'image') === FALSE) {
              // Collect the name of the field
              $field_name_list[$field_name] = TRUE;
            }
          } 
          // Care for non-field chained tokens, but ONLY if the entity is not a core entity.
          elseif (!_entity_token_is_core_entity($type)) {
            $property_name_list[$field_name] = TRUE;
          }
        }
      }
    }

    // Now process tokens for each of the chained fields collected above
    foreach (array_keys($field_name_list) as $field_name) {
      $chained_tokens = token_find_with_prefix($tokens, $field_name);
      if (isset($wrapper->$field_name)) {
        $replacements += token_generate('wrapped-field', $chained_tokens, array('wrapped-field' => $wrapper->$field_name), $options);
      }
    } 

    // Now process tokens for each of the chained properties collected above
    foreach (array_keys($property_name_list) as $property_name) {
      $chained_type = entity_token_determine_type($entity_properties[$property_name]);
      $property_tokens = token_find_with_prefix($tokens, $property_name);
      $replacements += token_generate($chained_type, $property_tokens, array($chained_type => $wrapper->{$property_name}->value()), $options);
    }
  } // End of process for $data that's an entity keyed by entity type.
  
  // Process tokens of type 'wrapped-field', generated by the above code.
  // The data passed here is a metadata wrapper of a field
  // At the moment only CHAINED tokens are passed here (except those for image and date fields, which are handled by core)
  // since unchained field tokens are always handled by core.
  // The $tokens variable starts with the first property or delta to be fulfilled.
  elseif ($type == 'wrapped-field' && isset($data[$type]) && !empty($data[$type]) && is_a($data[$type], 'EntityMetadataWrapper')){
    $wrapper = $data['wrapped-field'];

    $token_type_from_wrapper = entity_token_determine_type($wrapper->info());
    foreach ($tokens as $name => $original) {
      // $name is comprised delta:property:etc or property:etc
      $name_parts = explode(':', $name);
      $replacement = '';
      if (empty($name_parts[0]) && $name_parts[0] != '0') {
        continue;
      }

      if ($token_type_from_wrapper == 'file') {
        // Get the file object from the wrapper. We send the object (not the wrapper) to core
        // file tokens fulfilled in system_tokens().
        $new_data = $wrapper->file->value();

      } elseif (empty($token_type_from_wrapper)) {
        // We are dealing with text. Generate the replacement right here
        try { 
          if (isset($wrapper->{$name_parts[0]})) {
            $replacement = _entity_token_get_token($wrapper->{$name_parts[0]}, $options);
          }
        } catch (EntityMetadataWrapperException $e) {
          // if this doesn't work, do nothing
        }

      } elseif ($token_type_from_wrapper == 'struct') {
        // If this is a struct we'll pass the wrapper verbatim
        $new_data = $wrapper; 

      } elseif (strpos($token_type_from_wrapper, 'list')!== FALSE) {
        // Token types that include list are multi-value fields
        // Only pass it if the delta is numeric
        if (is_numeric($name_parts[0])) {
          $new_data = $wrapper;
        }
      }

      if (!empty($replacement) || (string) $replacement === '0') {
        // Produce the replacement here
        $replacements[$original] = $replacement;
      } elseif (!empty($new_data)) {
        // Generate tokens for struct and list<something>
        $replacements += token_generate($token_type_from_wrapper, array($name => $original), array($token_type_from_wrapper => $new_data), $options);
      }
    }
  } // End of process for wrapped-field

  // Process tokens that are of type list<something>. 
  // These are regular arrays, arrays of entities, or arrays of structures.
  // We get here after the 'wrapped-field' type has processed
  // In this case, $data keyed by list<something> and 
  // subarray of entities keyed by deltas
  elseif (!empty($data[$type]) && strpos($type, 'list') !== FALSE  && is_a($data[$type], 'EntityMetadataWrapper')) {
    $token_type = entity_plus_property_list_extract_type($type);

    foreach ($tokens as $name => $original) {
      $name_parts = explode(':', $name);
      $chained_tokens = token_find_with_prefix($tokens, $name_parts[0]);

      // If there are not chained tokens after the delta, we attempt to fulfill
      // the value replacement here 
      if (empty($chained_tokens)) {
        if (is_numeric($name)) { // be sure this is a real delta
          try {
            $wrapper = $data[$type]->get($name_parts[0]);
                
            if (isset($wrapper)) {
              $replacement = _entity_token_get_token($wrapper, $options);
              if (isset($replacement)) {
                $replacements[$original] = $replacement;
              }
            }
          }
          catch (EntityMetadataWrapperException $e) {
            // If getting values generate an error, do nothing. This may happen if the property info is not defined
          }
        }
      } 
      
      // If there are chained elements after the delta, pass that to token_generate 
      elseif (is_numeric($name_parts[0])) {
        $pass_data = '';
        // Check to be sure the delta value exists.
        try {
          switch ($token_type) {
            case 'struct':
              // We pass the wrapper to the element.
              $pass_data = $data[$type]->get($name_parts[0]);
              break;
  
            case 'file':
              // We need to pass the file object.
              $pass_data = $data[$type]->get($name_parts[0])->file->value();
              break;

            default:
              // We pass the value, since most likely this is not fulfilled in this module
              $pass_data = _entity_token_get_token($data[$type]->get($name_parts[0]), $options);
              break;
          }
        } catch (EntityMetadataWrapperException $e) {
          // Do nothing if the property doesn't exist
        }
        
        if (!empty($pass_data)) {
          $replacements += token_generate($token_type, $chained_tokens, array($token_type => $pass_data), $options);
        }
      }
    }
  } // End of process for token types that are list<something>

  // Process wrapped struct token types
  // These are associative arrays of properties
  elseif ($type == 'struct' && isset($data[$type]) && !empty($data[$type])) {
    $wrapper = $data['struct'];
    if (is_a($wrapper, 'EntityMetadataWrapper')) {
      foreach ($tokens as $name => $original) {
        $value = '';
        $name_parts = explode(':', $name);
        foreach ($name_parts as $name_part) {
          if (!isset($wrapper->{$name_part})) {
            continue 2;
          }
          $wrapper = $wrapper->{$name_part};
        }
        try {
          $value = _entity_token_get_token($wrapper, $options);
        } catch (EntityMetadataWrapperException $e) {

        }
        if (!empty($value) || (string) $value === '0') {
          $replacements[$original] = $value;
        }
      }
    }
  }

  return $replacements;
}


/** 
 * Returns a list of entity tokens that are not provided
 * by core.
 */
function entity_token_legacy_properties() {
  return array(
    'is_new',
    'status',
    'promote',
    'sticky',
    'revision',
    'subject',
    'comment_body',
    'current_user',
    'current_page',
    'parents_all',
    'weight',
    'last_access',
    'theme',
  );
}

/**
 * Gets and formats the token replacement
 */
function _entity_token_get_token($wrapper, $options) {
  if (!$wrapper || $wrapper->value() === NULL) {
    // Do not provide a replacement if there is no value.
    return NULL;
  }

  if (empty($options['sanitize'])) {
    // When we don't need sanitized tokens decode already sanitizies texts.
    $options['decode'] = TRUE;
  }
  $langcode = isset($options['language']) ? $options['language']->langcode : NULL;

  // If there is a label for a property, e.g. defined by an options list or an
  // entity label, or a taxonomy term, make use of it.
  if ($label = $wrapper->label()) {
    return empty($options['sanitize']) ? $label : check_plain($label);
  }

  switch ($wrapper->type()) {
    case 'integer':
      return $wrapper->value();
    case 'decimal':
      return number_format($wrapper->value(), 2);
    case 'date':
      return format_date($wrapper->value(), 'medium', '', NULL, $langcode);
    case 'duration':
      return format_interval($wrapper->value(), 2, $langcode);
    case 'boolean':
      return $wrapper->value() ? t('true') : t('false');
    case 'uri':
    case 'text':
      return $wrapper->value($options);
  }

  // Care for outputing list values.
  if ($wrapper instanceof EntityListWrapper) {
    $output = array();
    foreach ($wrapper as $item) {
      $output[] = _entity_token_get_token($item, $options);
    }
    return implode(', ', $output);
  }
  // Else we do not have a good string to output, e.g. for struct values. Just
  // output the string representation of the wrapper.
 return (string) $wrapper;
}

/**
 * Helper to determine the token type based on the property info
 */
function entity_token_determine_type($property_info) {
  $property_type = !empty($property_info['type']) ? $property_info['type'] : 'text';
  $is_list = strpos($property_type, 'list') !== FALSE;
  if ($is_list) {
    $property_type = entity_plus_property_list_extract_type($property_type);
  } 
  $token_type = _entity_token_get_token_type_from_property_type($property_type);

  return $is_list ? "list<$token_type>" : $token_type;
}

/** 
 * Helper to return the known types based on field type
 */
function _entity_token_get_token_type_from_property_type($field_type) {
  switch ($field_type) {
    case 'taxonomy_term':
      return 'term';
      break;

    case 'datetime':
    case 'date':
    case 'datestamp':
      return 'date';
      break;

    case 'field_item_file':
      return 'file';
      break;

    case 'text_formatted':
    case 'field_item_link':
      return 'struct';
      break;

    case 'field_item_image':
      return 'image';
      break;

    default:
      return $field_type;
  }
}

/** 
 * Helper to get the entity type from token type
 * For most this is the same, except (in core) for token term
 * @todo rewrite to build this list from entity_info
 */
function _entity_token_get_entity_from_token($token_type) {
  switch ($token_type) {
    case 'term':
      return 'taxonomy_term';
      break;

    case 'date':
      return 'date';
      break;

    case 'node':
      return 'node';
      break;

    case 'comment':
      return 'comment';
      break;

    case 'link':
      return 'link';
      break;

    case 'site':
      return 'site';
      break;

    default:
      // for text for example
      return NULL;
  }
}

/** 
 * Check if entity type is one of the core entities.
 * 
 * @param string $entity_type
 *   The entity type.
 * 
 * @return bool
 *   Whether the entity type is one of the core entities.
 */
function _entity_token_is_core_entity($entity_type) {
  return in_array($entity_type, array(
    'node',
    'file',
    'comment',
    'user',
    'taxonomy_term',
  )
  );
}
