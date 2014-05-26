<?php

/**
 * @file
 * @author Nigel Banks
 *
 * Defines a FormDefinition class that allows XML form defintions to be processed by the FormGenerator, and other
 * classes.
 */
module_load_include('inc', 'php_lib', 'String');
module_load_include('inc', 'php_lib', 'Array');

/**
 * Encapulates the version number and the transform's/schema's related to the version number of a given
 * XML Form Definition.
 */
class XMLFormDefinitionVersion {

  /**
   * The version which this class repersents.
   * 
   * @var int
   */
  private $version;

  /**
   * The path to the directory where all the schema definitions are stored.
   * 
   * @return string
   */
  public static function getPathToSchemaDefinitionDirectory() {
    return drupal_get_path('module', 'xml_form_api') . '/data/schemas';
  }

  /**
   * The path to the directory where all the XSL Transformations are stored.
   * 
   * @return string
   */
  public static function getPathToXSLTransformDirectory() {
    return drupal_get_path('module', 'xml_form_api') . '/data/transforms';
  }

  /**
   * Gets a list of all the known versions of a XML Form Definition.
   * 
   * @return Array
   *   An array where all the values correspond to the defined versions of an XML Form Definition. 
   *   In order from oldest to most recent.
   */
  public static function getAllVersions() {
    static $versions = NULL;
    if (empty($versions)) {
      $versions = array();
      $path = self::getPathToSchemaDefinitionDirectory();
      $files = scandir($path);
      foreach ($files as $filename) {
        if (preg_match('/^.*\.xsd$/', $filename)) {
          $version = str_replace('.xsd', '', $filename);
          $versions[] = (int) $version;
        }
      }
      asort($versions);
    }
    return $versions;
  }

  /**
   * Checks to see if the given version number is valid.
   * 
   * Valid schema numbers are defined by having a corresponding Schema file with the same version.
   * 
   * @param int $version 
   *   A XML Form Definition version number.
   * 
   * @return bool
   */
  public static function isValid($version) {
    $versions = self::getAllVersions();
    return array_search($version, $versions) !== FALSE;
  }

  /**
   * Gets the latest schema version number.
   * 
   * @return int
   *   The latest version number.
   */
  public static function getLatestVersion() {
    $versions = self::getAllVersions();
    $latest_version = array_pop($versions);
    return new XMLFormDefinitionVersion($latest_version);
  }

  /**
   * Gets the version number of the XML Form Definition.
   * 
   * Also validates the document against that version number.s
   * 
   * @param DOMDocument $definition
   *   The XML Form Definition.
   * 
   * @return float
   *   The version of the XML Form Definition, if the XML Form Definition is not valid FALSE is returned.
   */
  public static function getVersion(DOMDocument $definition) {
    $declares_version = $definition->documentElement->hasAttribute('version');
    if ($declares_version) {
      $version = (int) $definition->documentElement->getAttribute('version');
      return self::isValid($definition, $version);
    }
    else { // Files with a version of 0 or 1 may not have their version declared. Check to see manually.
      $undeclared_versions = array(
        new XMLFormDefinitionVersion(0),
        new XMLFormDefinitionVersion(1)
      );
      foreach ($undeclared_versions as $version) {
        if (self::isValid($definition, $version)) {
          return $version;
        }
      }
    }
    return FALSE; // Could not find the version.
  }

  /**
   * Creates an instance of the XMLFormDefinitionVersion.
   * 
   * @param int $version
   *   The XML Form Definition Schema number.
   */
  public function __construct($version) {
    if (self::isValid($version)) {
      $this->version = $version;
    }
  }

  /**
   * Gets the numerical repersentation of this version.
   * 
   * @return int
   */
  public function get() {
    return $this->version;
  }

  /**
   * Gets the next most recent version number from the version number given.
   * 
   * @return XMLFormDefinitionVersion
   *   The next version number if the given version is not the latest, otherwise FALSE is returned.
   */
  public function getNextVersion() {
    $versions = self::getAllVersions();
    $position = array_search($this->get(), $versions);
    $next_position = ++$position;
    if (isset($versions[$next_position])) {
      $next_version = $versions[$next_position];
      return new XMLFormDefinitionVersion($next_version);
    }
    return FALSE;
  }

  /**
   * Checks to see if the XML Form Definition is at the most recent version.
   * 
   * @return bool
   *   TRUE if it is the latest, FALSE otherwise.
   */
  public function isLatestVersion() {
    $latest = self::getLatestVersion();
    return $this->get() == $latest->get();
  }

  /**
   * Gets the filename of the Schema that repersents this version.
   * 
   * @return string
   */
  public function getSchemaFileName() {
    $path = self::getPathToSchemaDefinitionDirectory();
    return $path . '/' . $this->version . '.xsd';
  }

  /**
   * Gets the filename of the XSL Transform that repersents this version.
   * 
   * @return string
   */
  public function getTansformFileName() {
    $path = self::getPathToXSLTransformDirectory();
    return $path . '/' . $this->version . '.xsl';
  }

  /**
   * Gets an XSLTProcessor that has been initialized with the transform specified by $version.
   * 
   * @param int $version 
   */
  public function getTransform() {
    $filename = $this->getTansformFileName();
    if (file_exists($filename)) {
      $xsl = new DOMDocument();
      $xsl->load($filename);
      $xslt = new XSLTProcessor();
      $xslt->importStyleSheet($xsl);
      return $xslt;
    }
    return FALSE;
  }

}

/**
 * This class is used to validate and transform XML Form Definitions into its Drupal array based repersentation.
 */
class XMLFormDefinition {

  /**
   * The DOMDocument that repersents the Form Definition.
   * 
   * @var DOMDocument
   */
  public $definition;

  /**
   * Gets the version number of the XML Form Definition.
   * 
   * Also validates the document against that version number.s
   * 
   * @throws Exception
   *  If the Version can not be determined or if the definition is not valid.
   * 
   * @param DOMDocument $definition
   *   The XML Form Definition.
   * 
   * @return float
   *   The version of the XML Form Definition, if the XML Form Definition is not valid FALSE is returned.
   */
  public static function getVersion(DOMDocument $definition) {
    if (empty($definition->documentElement)) {
      throw new Exception(t('XML Form Definition is not valid.'));
    }
    $declares_version = $definition->documentElement->hasAttribute('version');
    if ($declares_version) {
      $version = (int) $definition->documentElement->getAttribute('version');
      $version = new XMLFormDefinitionVersion($version);
      if (self::isValid($definition, $version)) {
        return $version;
      }
      throw new Exception(t('XML Form Definition is not valid.'));
    }
    else { // Files with a version of 0 or 1 may not have their version declared. Check to see manually.
      $undeclared_versions = array(
        new XMLFormDefinitionVersion(0),
        new XMLFormDefinitionVersion(1)
      );
      foreach ($undeclared_versions as $version) {
        if (self::isValid($definition, $version)) {
          return $version;
        }
      }
    }
    throw new Exception(t('Failed to determine the version of the XML Form Definition')); // Could not find the version.
  }

  /**
   * Uses the XML Schema associated with the XML Form Definitions version to validate the definition
   * 
   * @param DOMDocument $definition
   *   The XML Form Definition.
   * @param XMLFormDefinitionVersion $version 
   *   The schema version, if none is give the latest is used. 
   * 
   * @return boolean
   *   TRUE if the XML Form Definition is valid, FALSE otherwise.
   */
  public static function isValid(DOMDocument $definition, XMLFormDefinitionVersion $version = NULL) {
    $version = isset($version) ? $version : XMLFormDefinitionVersion::getLatestVersion();
    $file_name = $version->getSchemaFileName();
    return @$definition->schemaValidate($file_name);
  }

  /**
   * Incrementally upgrades the XML Form Definition, to its next version after its own.
   * 
   * If the document is already at its latest version nothing occurs.
   *
   * @param DOMDocument $definition
   *   The XML Form Definition to upgrade. This may be modified by this function.
   * 
   * @return XMLFormDefinitionVersion
   *   The upgraded version of the XML Form Definition.
   */
  public static function upgradeVersion(DOMDocument &$definition) {
    $current = self::getVersion($definition);
    $next = $current->getNextVersion();
    if ($next) { // There exists a version to upgrade to.
      $xslt = $next->getTransform();
      $definition = $xslt->transformToDoc($definition->documentElement);
      return $next;
    }
    return $current; // Do nothing
  }

  /**
   * Repeatedly upgrades the XML Form Definition, till it reaches the latest version.
   * 
   * @param DOMDocument $definition 
   *   The XML Form Definition to upgrade.
   * 
   * @return XMLFormDefinition
   *   The upgraded XML Form Definition.
   */
  public static function upgradeToLatestVersion(DOMDocument $definition) {
    do {
      $version = self::upgradeVersion($definition);
    } while (!$version->isLatestVersion());
    return $definition;
  }

  /**
   * Creates an instance of the XMLFormDefinition.
   * 
   * @param DOMDocument $definition 
   */
  public function __construct(DOMDocument $definition) {
    $this->definition = self::upgradeToLatestVersion($definition);
    if (!self::isValid($this->definition)) {
      throw new Exception('Unable to Create XMLFormDefinition.');
    }
  }

  /**
   * Gets the definition.
   * 
   * @return DOMDocument
   *  
   */
  public function get() {
    return $this->definition;
  }

  /**
   * Extracts the form section of the definition into a array.
   * 
   * @return array
   *   The extracted form declaration from the XML Form Definition. This is also a valid Drupal form.
   * FBA MODIFICATION BY NBYWELL on 16/01/2014 to collect
   * autocomplete data that is necessary for object linking.
   * The currentPath info is not actually used, but it is
   * useful when debugging.
   */
  public function getForm() {
    if (variable_get('rdf_relationships', NULL)) {
      variable_set('rdf_relationships', NULL);
    }
    $definition = simplexml_import_dom($this->definition);
    $currentPath = "";
    $depth = 0;
    return $this->getElement($definition->form, $currentPath, $depth);
  }

  /**
   * Creates an XMLDocument instance from the properties stored within the XML Form Definition.
   * 
   * @param string $xml 
   *   The metadata to initalize the XMLDocument with.
   * 
   * @return XMLDocument
   *   An initialized XMLDocument.
   */
  public function createXMLDocument($xml = NULL) {
    module_load_include('inc', 'xml_form_api', 'XMLDocument');
    $properties = $this->getProperties();
    $root_name = $properties['root_name']; // @todo change to name.
    $schema_uri = isset($properties['schema_uri']) ? $properties['schema_uri'] : NULL;
    $default_uri = isset($properties['default_uri']) ? $properties['default_uri'] : NULL;
    $namespaces = new Namespaces($properties['namespaces'], $default_uri);
    return new XMLDocument($root_name, $namespaces, $schema_uri, $xml);
  }

  /**
   * Extracts the properties section of the definition into a array.
   * 
   * @return array
   *   The extracted properties from the XML Form Definition.
   */
  public function getProperties() {
    $paths = array(
      'root_name' => '/definition/properties/root_name',
      'default_uri' => '/definition/properties/namespaces/@default',
      'schema_uri' => '/definition/properties/schema_uri'
    );
    $properties = array();
    $xpath = new DOMXPath($this->definition);
    foreach ($paths as $key => $path) {
      $results = $xpath->query($path);
      if ($results->length) {
        $properties[$key] = $results->item(0)->nodeValue;
      }
    }
    $results = $xpath->query('/definition/properties/namespaces/namespace');
    $count = $results->length;
    for ($i = 0; $i < $count; $i++) {
      $node = $results->item($i);
      $prefix = $node->getAttribute('prefix');
      $properties['namespaces'][$prefix] = $node->nodeValue;
    }
    return $properties;
  }

  /**
   * Gets the array repersentation of an Element from the XML Form Definition.
   * 
   * @param SimpleXMLElement $element 
   *   The element declaration to transform into its Drupal form.
   * 
   * @return array
   *   The Drupal form equivalent of the XML Form Definition element declaration.
   * FBA MODIFICATION BY NBYWELL on 16/01/2014 to collect autocomplete data that is necessary for object linking.
   */

  private function getElement(SimpleXMLElement $element, &$currentPath, &$depth) {
    $properties = $this->getElementProperties($element, $currentPath, $depth);
    return array_merge($properties, $this->getElementChildren($element, &$currentPath, &$depth));
  }

  /**
   * Transforms the properties section of an element declaration to its drupal repersentation.
   * 
   * @param SimpleXMLElement $element
   *   An element declaration from the XML Form Definition.
   * 
   * @return array
   *   The Drupal repersentation of this element's properties declaration.
   *
   * FBA MODIFICATION BY NBYWELL on 16/01/2014 to collect autocomplete data that is necessary for object linking.
   */

  private function getElementProperties(SimpleXMLElement $element, &$currentPath, &$depth) {
    $output = array();
    $autocompleteProperty = 'autocomplete_path';
    $fieldIdentifier = '';

    if (isset($element->attributes()->name)) {
      $attributeName = self::getElementPropertyName($element->attributes()->name);
      if (!strcmp($attributeName, 'name')) {
        $fieldIdentifier = $this->getElementProperty($element->attributes()->name);
      }
    }

    if (isset($element->properties)) {
      $properties = $element->properties->children();
      $localPath = "";
      $autocompleteFound = false;
      $currentPathComponent = "";
      $previousPathComponent = "";
      foreach ($properties as $property) {
        $name = self::getElementPropertyName($property);
        $content = $this->getElementProperty($property);
        $output["#$name"] = $content;

        // The following code adds the field identifier of the current element
        // to the currentPath. If there is an autocomplete associated with
        // current element it checks to see if the autocomplete is one for
        // which object-linking is required. If it is, the relevant data
        // is stored in the superglobals array (not ideal!) so that it can be
        // used in the post-processing of the form.

        if (!strcmp($name, 'type')) {
          if (!strcmp($content,'tabs')) {
            $currentPath = $currentPath . '/' . $fieldIdentifier;
          }
        }
        elseif (!strcmp($name, 'actions')) {
          $actions = $property->children();
          foreach ($actions as $action) {
            $actionName = self::getElementPropertyName($action);
            if (!strcmp($actionName,'create')) {
              $createComponents = $action->children();
              foreach ($createComponents as $createComponent) {
                $createComponentName = self::getElementPropertyName($createComponent);
                if (!strcmp($createComponentName,'value')) {
                  $currentPathComponent = self::getElementProperty($createComponent);
                  if (strcmp($currentPathComponent,$previousPathComponent)) {
                    // Fieldset, markup, tabpanel etc can have multiple
                    // fields at the same depth. Therefore, test whether
                    // this is the last field in the path. If it is, do not
                    // append the field identifier to the currentPath as it
                    // will not form part of the currentPath of other fields
                    // at this depth. Store it in localPath instead.

                    if (empty($element->children)) {
                      $localPath = $currentPath . '/' . $fieldIdentifier;
                    } else {
                      $currentPath = $currentPath . '/' . $fieldIdentifier;
                    }
                    if ($autocompleteFound && (empty($element->children))) {
                      $autocompleteFound = false;

                      if ($autocompleteData = self::getAutocompleteData($autocompleteName)) {
                        foreach ($autocompleteData as $autocompleteFieldName => $autocompleteFieldValue) {
                          if (!strcmp($autocompleteFieldName,"rdf_relationship")) {
                            $rdfRelation = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_pid_append_required")) {
                            $object_pid_append_required = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"sparql_field")) {
                            $sparql_field = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"subject_dsid")) {
                            $subject_dsid = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"subject_namespace_info")) {
                            $subject_namespace_info = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"subject_xpath_title_search")) {
                            $subject_xpath_title_search = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"subject_xpath_data_search")) {
                            $subject_xpath_data_search = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_creation_required")) {
                            $object_creation_required = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"inverse_required")) {
                            $inverse_required = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"symmetry_in_object_datastream_required")) {
                            $symmetry_in_object_datastream_required = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_dsid")) {
                            $object_dsid = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_namespace_info")) {
                            $object_namespace_info = $autocompleteFieldValue;


                          } else if (!strcmp($autocompleteFieldName,"object_datastream_xml")) {
                            $object_datastream_xml = $autocompleteFieldValue;


                          } else if (!strcmp($autocompleteFieldName,"object_xpath_title_search")) {
                            $object_xpath_title_search = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_xpath_date_create_search")) {
                            $object_xpath_date_create_search = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_content_model_pid")) {
                            $object_content_model_pid = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_collection_pid")) {
                            $object_collection_pid = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_namespace_prefix")) {
                            $object_namespace_prefix = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_transform")) {
                            $object_transform = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_state")) {
                            $object_state = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_xpath_data_search")) {
                            $object_xpath_data_search = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_xpath_data_build")) {
                            $object_xpath_data_build = $autocompleteFieldValue;
                          } else if (!strcmp($autocompleteFieldName,"object_xpath_data_build_countback")) {
                            $object_xpath_data_build_countback = $autocompleteFieldValue;
                          }
                        }
                        // Store the values using the variable_set function so
                        // they they are available in the post processing of
                        // the form.

                        $rdfRelationshipsUnserialized = variable_get('rdf_relationships', NULL);
                        $rdfRelationshipsUnserialized[] = array("form_token"=>$_POST[form_token], "field_path"=>$localPath, "rdf_predicate"=>$rdfRelation, "object_pid_append_required"=>$object_pid_append_required, "sparql_field"=>$sparql_field, "subject_dsid"=>$subject_dsid, "subject_namespace_info"=>$subject_namespace_info, "subject_xpath_title_search"=>$subject_xpath_title_search, "subject_xpath_data_search"=>$subject_xpath_data_search, "object_creation_required"=>$object_creation_required, "inverse_required"=>$inverse_required, "symmetry_in_object_datastream_required"=>$symmetry_in_object_datastream_required, "object_dsid"=>$object_dsid, "object_namespace_info"=>$object_namespace_info, "object_datastream_xml"=>$object_datastream_xml, "object_xpath_title_search"=>$object_xpath_title_search, "object_xpath_date_create_search"=>$object_xpath_date_create_search, "object_content_model_pid"=>$object_content_model_pid, "object_collection_pid"=>$object_collection_pid, "object_namespace_prefix"=>$object_namespace_prefix, "object_transform"=>$object_transform, "object_state"=>$object_state, "object_xpath_data_search"=>$object_xpath_data_search, "object_xpath_data_build"=>$object_xpath_data_build, "object_xpath_data_build_countback"=>$object_xpath_data_build_countback);
                        variable_set('rdf_relationships', $rdfRelationshipsUnserialized);
                      }
                    }
                    $previousPathComponent = $currentPathComponent;
                  }
                }
              }
            }
          }
        } elseif (!strcmp($name, $autocompleteProperty) && !$autocompleteFound) {
          $autocompleteFound = true;
          $autocompleteName = self::getElementProperty($property);
          if (strlen($autocompleteName) > strlen('autocomplete/')) {
            $autocompleteName = substr($autocompleteName,strlen('autocomplete/'));
          }
        }
      }
    }
    return $output;
  }

  /**
   * Get the data relating to the autocomplete from the database
   * 
   * @param string $autcompleteName
   *   The name that identifies the autocomplete row in the table
   * 
   * @return array
   *   Contains the rdf_relation data.
   *
   * FBA MODIFICATION BY NBYWELL on 21/01/2014
   */

  private function getAutocompleteData($autocompleteName) {

    $result = db_query('select ias.rdf_relationship,ias.object_pid_append_required,ias.sparql_field,ias.subject_dsid,ias.subject_namespace_info,ias.subject_xpath_title_search,ias.subject_xpath_data_search,ias.object_creation_required,ias.inverse_required,ias.symmetry_in_object_datastream_required,ias.object_dsid,ias.object_namespace_info,ias.object_datastream_xml,ias.object_xpath_title_search,ias.object_xpath_date_create_search,ias.object_content_model_pid,ias.object_collection_pid,ias.object_namespace_prefix,ias.object_transform,ias.object_state,ias.object_xpath_data_search,ias.object_xpath_data_build,ias.object_xpath_data_build_countback from islandora_autocomplete ia join islandora_autocomplete_sparql ias on ia.id=ias.url_id where ia.name like "%s" and ia.source = "sparql" and ias.rdf_relationship <> ""', $autocompleteName);
    return $result !== FALSE ? db_fetch_object($result) : FALSE;
  }

  /**
   * Gets the value of a property declared within the properties section of an element declaration 
   * from the XML Form Definition.
   * 
   * @param SimpleXMLElement $property
   *   An element property declaration from the XML Form Deinition.
   * 
   * @return mixed
   *   The value of the property.
   */
  private function getElementProperty(SimpleXMLElement $property) {
    $children = $property->children();
    if (count($children) == 0) {
      return cast_string_to_type((string) $property); // If cast fails the string is returned.
    }
    $output = array();
    foreach ($children as $child) {
      $name = cast_string_to_type((string) $this->getElementPropertyName($child));
      $output[$name] = $this->getElementProperty($child);
    }
    return $output;
  }

  /**
   * Gets the name of the property to use as the array index of the property in the Drupal form.
   * 
   * @param SimpleXMLElement $property
   *   An element property declaration from the XML Form Definition.
   * 
   * @return string
   *   The property name.
   */
  private function getElementPropertyName(SimpleXMLElement $property) {
    if (isset($property['key'])) {
      $key = (string) $property['key'];
      return strcasecmp($key, 'NULL') == 0 ? NULL : $key;
    }
    return $property->getName();
  }

  /**
   * Transforms all the child declarations of the given element into their Drupal form repersentations.
   * 
   * @param SimpleXMLElement $element
   *   The element declaration to transform into its Drupal form.
   * 
   * @return array 
   *  All the transformed child declarations.
   *
   * FBA MODIFICATION BY NBYWELL on 16/01/2014 to collect autocomplete data that is necessary for object linking.
   */

  private function getElementChildren(SimpleXMLElement $element, &$currentPath, &$depth) {
    $output = array();
    if (isset($element->children)) {
      $depth += 1;
      $elements = $element->children->children();
      foreach ($elements as $element) {
        $key = isset($element['name']) ? (string) $element['name'] : NULL;
        array_add($output, $key, $this->getElement($element, $currentPath, $depth));
      }
      $depth -= 1;
      if ($depth == 1) {
        $currentPath = "";
      }
    }
    return $output;
  }
}

/**
 * Used to create a XML Form Definition from a Drupal form and an array of properties.
 */
class XMLFormDefinitionGenerator {

  /**
   * Creates an DOMDocument that defines an XML Form.
   * 
   * @param array $properties
   *   The form properties.
   * 
   * @param array $form 
   *   A Drupal form.
   *
   * @return DOMDocument
   *   An XML Form Definition.
   */
  public static function Create(array &$properties, array &$form) {
    $latest = XMLFormDefinitionVersion::getLatestVersion();
    $definition = new SimpleXMLElement("<definition xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' version='{$latest->get()}'/>");
    self::AddProperties($definition, $properties);
    //@todo no registry defined...
    self::AddElement($definition->addChild('form'), new FormElement(NULL, $form));
    $xml = $definition->asXML();
    $definition = new DOMDocument();
    $definition->loadXML($xml);
    return $definition;
  }

  /**
   * Add's the properties to the root element, of the XML Form Definition.
   * 
   * @param SimpleXMLElement $definition 
   *   The root element of the XML Form Definition.
   * @param array $properties
   *   The form properties.
   */
  private static function AddProperties(SimpleXMLElement $definition, array &$properties) {
    $form_properties = $definition->addChild('properties');
    if (isset($properties['root_name'])) {
      $form_properties->addChild('root_name', $properties['root_name']);
    }
    if (isset($properties['schema_uri'])) {
      $form_properties->addChild('schema_uri', $properties['schema_uri']);
    }
    $namespaces = $form_properties->addChild('namespaces');
    if ($properties['default_uri']) {
      $namespaces->addAttribute('default', $properties['default_uri']);
    }
    if (isset($properties['namespaces'])) {
      foreach ($properties['namespaces'] as $prefix => $uri) {
        $namespace = $namespaces->addChild('namespace', $uri);
        $namespace->addAttribute('prefix', $prefix);
      }
    }
  }

  /**
   * Addes the form to the root element, of the XML Form Definition.
   * 
   * @param SimpleXMLElement $definition
   *   The root element of the XML Form Definition.
   * @param FormElement $form
   *   The Form.
   */
  private static function AddElement(SimpleXMLElement $parent, FormElement $element) {
    $properties = $parent->addChild('properties');
    foreach ($element->controls as $key => $value) {
      self::AddElementProperty($properties, $key, $value);
    }
    $children = $parent->addChild('children');
    foreach ($element->children as $key => $child) {
      $element = $children->addChild('element');
      $element->addAttribute('name', $key);
      self::AddElement($element, $child);
    }
  }

  /**
   * Adds a element property to the properties section of an element declaration.
   * 
   * @param SimpleXMLElement $properties
   *   The properties section of an element declaration.
   * @param string $key
   *   The properties name.
   * @param mixed $value 
   *   The properties value.
   */
  private static function AddElementProperty(SimpleXMLElement $properties, $key, $value) {
    $property = self::CreateElementProperty($properties, $key);
    self::SetElementProperty($property, $value);
  }

  /**
   * Creates an XML Tag repersentation of propery.
   *  
   * @param SimpleXMLElement $properties
   *   The parent to which the new property will belong to.
   * @param string $key
   *   The potential tag name for the newly created property.
   * 
   * @return SimpleXMLElement
   *   The created Element Property.
   */
  private static function CreateElementProperty(SimpleXMLElement $properties, $key) {
    $key = trim($key, '#');
    if (!self::IsValidXMLTag($key)) {
      $property = $properties->addChild('index');
      $property->addAttribute('key', cast_type_to_string($key));
      return $property;
    }
    return $properties->addChild($key);
  }

  /**
   * Sets the Element Property.
   * 
   * @param SimpleXMLElement $property
   *   The property whose value will be set.
   * @param mixed $value
   *   The value to assign to the $property.
   */
  private static function SetElementProperty(SimpleXMLElement $property, $value) {
    $value = ($value instanceof FormPropertyInterface) ? $value->toDrupalForm() : $value;
    if (is_array($value)) {
      foreach ($value as $key => $item) {
        self::AddElementProperty($property, $key, $item);
      }
    }
    else {
      $property->{0} = cast_type_to_string($value);
    }
  }

  /**
   * Checks to see if the given tag name can be used for an XML tags.
   * 
   * Numbers are invalid XML tags.
   * 
   * @param string $tag_name
   *   The proposed tag name.
   * @return type 
   *   TRUE if the name is valid FALSE otherwise.
   */
  private static function IsValidXMLTag($tag_name) {
    // start [A-Z] | "_" | [a-z]
    // everything else is start plus | "-" | "." | [0-9] 
    return (preg_match('/^[a-zA-Z_][a-zA-Z0-9_]*$/', $tag_name) > 0) ? TRUE : FALSE;
  }

}
