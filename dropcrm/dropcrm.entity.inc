<?php
/**
 * @file
 * Entity class and controller definitions for DropCRM.
 *
 * All three entity classes extend Backdrop core's Entity abstract class and
 * implement the required EntityInterface methods. Controllers extend
 * EntityPlusController from the entity_plus module.
 */

// ---------------------------------------------------------------------------
// CrmContact entity
// ---------------------------------------------------------------------------

/**
 * Entity class for CRM contacts (persons, organizations, households).
 */
class CrmContact extends Entity {

  /**
   * {@inheritdoc}
   */
  public function id() {
    return isset($this->cid) ? $this->cid : NULL;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'crm_contact';
  }

  /**
   * {@inheritdoc}
   *
   * Returns the bundle (crm_person, crm_organization, or crm_household).
   */
  public function bundle() {
    return isset($this->type) ? $this->type : 'crm_person';
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/crm/contact/' . $this->id(), 'options' => array());
  }

}

/**
 * Controller class for CRM contacts.
 */
class CrmContactController extends EntityPlusController {
}

// ---------------------------------------------------------------------------
// CrmContactMethod entity
// ---------------------------------------------------------------------------

/**
 * Entity class for CRM contact methods (email, telephone, address).
 */
class CrmContactMethod extends Entity {

  /**
   * {@inheritdoc}
   */
  public function id() {
    return isset($this->cmid) ? $this->cmid : NULL;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'crm_contact_method';
  }

  /**
   * {@inheritdoc}
   *
   * Returns the bundle (email, telephone, or address).
   */
  public function bundle() {
    return isset($this->type) ? $this->type : 'email';
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/crm/contact-method/' . $this->id(), 'options' => array());
  }

  /**
   * {@inheritdoc}
   *
   * Returns a human-readable label combining the label and value fields.
   */
  public function label() {
    $label = isset($this->label) ? $this->label : '';
    $value = isset($this->value) ? $this->value : '';
    if ($label && $value) {
      return $label . ': ' . $value;
    }
    return $value ? $value : t('Contact method');
  }

}

/**
 * Controller class for CRM contact methods.
 */
class CrmContactMethodController extends EntityPlusController {

  /**
   * {@inheritdoc}
   */
  public function save($entity, ?DatabaseTransaction $transaction = NULL) {
    // Timestamps handled by hook presave.
    // Primary flag enforcement: clear other primaries first.
    if (!empty($entity->is_primary) && !empty($entity->cid)) {
      db_update('crm_contact_method')
        ->fields(array('is_primary' => 0))
        ->condition('cid', $entity->cid)
        ->condition('type', $entity->type)
        ->execute();
    }
    return parent::save($entity, $transaction);
  }

}

// ---------------------------------------------------------------------------
// CrmActivity entity
// ---------------------------------------------------------------------------

/**
 * Entity class for CRM activities.
 */
class CrmActivity extends Entity {

  /**
   * {@inheritdoc}
   */
  public function id() {
    return isset($this->aid) ? $this->aid : NULL;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'crm_activity';
  }

  /**
   * {@inheritdoc}
   *
   * crm_activity has a single implicit bundle with the same name as the entity
   * type. No override needed; the base class returns entityType() by default.
   */

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/crm/activity/' . $this->id(), 'options' => array());
  }

}

/**
 * Controller class for CRM activities.
 */
class CrmActivityController extends EntityPlusController {
}

// ---------------------------------------------------------------------------
// EntityReference selection handler for crm_contact
// ---------------------------------------------------------------------------

/**
 * Entityreference selection handler for crm_contact entities.
 *
 * Extends the generic handler to set the label key to 'name', enabling
 * autocomplete filtering for entityreference fields that target crm_contact
 * (e.g. field_crm_employer). Without this, $label_key is NULL and the
 * handler returns an unfiltered list of 10 entities regardless of input.
 */
class EntityReferenceSelectionHandlerGeneric_crm_contact extends EntityReferenceSelectionHandlerGeneric {
  protected $label_key = 'name';
}

// ---------------------------------------------------------------------------
// Admin UI controllers
// ---------------------------------------------------------------------------

/**
 * UI controller for crm_contact entities.
 *
 * Extends EntityBundleableUIController to get bundle-aware add pages
 * (/add → bundle selection, /add/crm_person → per-bundle form), while
 * restoring the overview list page that EntityContentUIController removes.
 */
class CrmContactUIController extends EntityBundleableUIController {

  /**
   * {@inheritdoc}
   */
  public function hook_menu() {
    $items = parent::hook_menu();

    // EntityContentUIController (parent chain) removes the overview path at
    // $this->path. Put it back so admin/crm/contacts shows the entity list.
    $items[$this->path] = array(
      'title'            => 'CRM Contacts',
      'page callback'    => 'backdrop_get_form',
      'page arguments'   => array('crm_contact_overview_form', $this->entityType),
      'access callback'  => 'entity_ui_access',
      'access arguments' => array('view', $this->entityType),
    );

    // Ensure the per-bundle add forms include our admin file.
    $file_path = backdrop_get_path('module', 'dropcrm');
    foreach (array('/add', '/add/%') as $suffix) {
      if (isset($items[$this->path . $suffix])) {
        $items[$this->path . $suffix]['file']      = 'dropcrm.admin.inc';
        $items[$this->path . $suffix]['file path'] = $file_path;
      }
    }

    return $items;
  }

  /**
   * {@inheritdoc}
   */
  public function breadcrumb($entity = NULL, $op = NULL) {
    $crumb = array(
      l(t('Home'), '<front>'),
      l(t('Drop Suite'), 'drop'),
      l(t('CRM'), 'drop/crm'),
      l(t('Contacts'), 'drop/crm/contacts'),
    );
    if ($entity && !empty($entity->cid)) {
      $crumb[] = l(entity_label('crm_contact', $entity), 'drop/crm/contact/' . $entity->cid);
    }
    backdrop_set_breadcrumb($crumb);
  }

}

/**
 * UI controller for crm_contact_method entities.
 *
 * Same pattern as CrmContactUIController: bundle-aware add pages with the
 * overview list page restored.
 */
class CrmContactMethodUIController extends EntityBundleableUIController {

  /**
   * {@inheritdoc}
   */
  public function breadcrumb($entity = NULL, $op = NULL) {
    $crumb = array(
      l(t('Home'), '<front>'),
      l(t('Drop Suite'), 'drop'),
      l(t('CRM'), 'drop/crm'),
    );
    if ($entity && !empty($entity->cid)) {
      $contact = crm_contact_load($entity->cid);
      if ($contact) {
        $crumb[] = l(t('Contacts'), 'drop/crm/contacts');
        $crumb[] = l(entity_label('crm_contact', $contact), 'drop/crm/contact/' . $entity->cid);
      }
    }
    backdrop_set_breadcrumb($crumb);
  }

  /**
   * {@inheritdoc}
   */
  public function hook_menu() {
    $items = parent::hook_menu();

    $items[$this->path] = array(
      'title'            => 'CRM Contact Methods',
      'page callback'    => 'backdrop_get_form',
      'page arguments'   => array('crm_contact_method_overview_form', $this->entityType),
      'access callback'  => 'entity_ui_access',
      'access arguments' => array('view', $this->entityType),
    );

    $file_path = backdrop_get_path('module', 'dropcrm');
    foreach (array('/add', '/add/%') as $suffix) {
      if (isset($items[$this->path . $suffix])) {
        $items[$this->path . $suffix]['file']      = 'dropcrm.admin.inc';
        $items[$this->path . $suffix]['file path'] = $file_path;
      }
    }

    return $items;
  }

  /**
   * {@inheritdoc}
   *
   * Overrides the default overview table to add a Contact column.
   */
  public function overviewTable($conditions = array()) {
    $query = db_select('crm_contact_method', 'm')
      ->fields('m', array('cmid', 'cid', 'type', 'label', 'value'))
      ->orderBy('c.name', 'ASC')
      ->orderBy('m.type', 'ASC');
    $query->leftJoin('crm_contact', 'c', 'c.cid = m.cid');
    $query->addField('c', 'name', 'contact_name');

    $result = $query->execute();

    $bundle_labels = _dropcrm_contact_method_bundle_options();
    $rows = array();
    foreach ($result as $row) {
      $contact_cell = $row->cid
        ? l($row->contact_name ?: t('Unknown'), 'drop/crm/contact/' . $row->cid)
        : '—';
      $type_label = isset($bundle_labels[$row->type]) ? $bundle_labels[$row->type] : check_plain($row->type);
      $ops = l(t('Edit'), $this->path . '/' . $row->cmid . '/edit')
        . ' &middot; '
        . l(t('Delete'), $this->path . '/' . $row->cmid . '/delete');
      $rows[] = array(
        $contact_cell,
        $type_label,
        check_plain($row->label),
        check_plain($row->value),
        $ops,
      );
    }

    return array(
      '#theme'  => 'table',
      '#header' => array(t('Contact'), t('Type'), t('Label'), t('Value'), t('Operations')),
      '#rows'   => $rows,
      '#empty'  => t('No contact details have been added yet.'),
    );
  }

}

// ---------------------------------------------------------------------------
// DropProject entity
// ---------------------------------------------------------------------------

/**
 * Entity class for a project.
 */
class DropProject extends Entity {
  public $pid;
  public $name    = '';
  public $color   = '#1565c0';
  public $active  = 1;
  public $cid     = NULL;
  public $uid     = 0;
  public $created = 0;
  public $changed = 0;

  /** {@inheritdoc} */
  public function id() {
    return $this->pid;
  }

  /** {@inheritdoc} */
  public function entityType() {
    return 'drop_project';
  }

  /** {@inheritdoc} */
  protected function defaultLabel() {
    return $this->name;
  }

  /** {@inheritdoc} */
  public function uri() {
    return array('path' => 'drop/crm/projects/' . $this->pid . '/edit', 'options' => array());
  }

  /** {@inheritdoc} */
  protected function defaultUri() {
    return $this->uri();
  }
}

/**
 * Controller for DropProject entities.
 */
class DropProjectController extends EntityDatabaseStorageController {

  /** {@inheritdoc} */
  public function save($entity) {
    global $user;
    if (empty($entity->created)) {
      $entity->created = REQUEST_TIME;
    }
    $entity->changed = REQUEST_TIME;
    if (empty($entity->uid)) {
      $entity->uid = $user->uid;
    }
    return parent::save($entity);
  }
}

/**
 * UI controller for crm_activity entities.
 */
class CrmActivityUIController extends EntityDefaultUIController {

  /**
   * {@inheritdoc}
   */
  public function breadcrumb($entity = NULL, $op = NULL) {
    $crumb = array(
      l(t('Home'), '<front>'),
      l(t('Drop Suite'), 'drop'),
      l(t('CRM'), 'drop/crm'),
      l(t('Activities'), 'drop/crm/activities'),
    );
    if ($entity && !empty($entity->aid)) {
      $crumb[] = l(entity_label('crm_activity', $entity), 'drop/crm/activity/' . $entity->aid);
    }
    backdrop_set_breadcrumb($crumb);
  }

}
