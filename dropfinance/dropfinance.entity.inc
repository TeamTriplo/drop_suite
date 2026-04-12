<?php
/**
 * @file
 * Entity classes and controllers for Drop Finance.
 */

// ---------------------------------------------------------------------------
// Vendor
// ---------------------------------------------------------------------------

/**
 * Entity class for a Drop Finance vendor.
 */
class DropVendor extends Entity {

  /** @var int Vendor ID. */
  public $vid;

  /** @var string Vendor name. */
  public $name = '';

  /** @var string Optional notes. */
  public $notes = '';

  /** @var int Owner's uid. */
  public $uid = 0;

  /** @var int Created timestamp. */
  public $created = 0;

  /** @var int Changed timestamp. */
  public $changed = 0;

  /**
   * {@inheritdoc}
   */
  public function id() {
    return $this->vid;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'drop_vendor';
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultLabel() {
    return $this->name;
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/finance/vendors/' . $this->vid . '/edit', 'options' => array());
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultUri() {
    return array('path' => 'drop/finance/vendors/' . $this->vid . '/edit', 'options' => array());
  }

}

/**
 * Controller class for DropVendor entities.
 */
class DropVendorController extends EntityDatabaseStorageController {

  /**
   * {@inheritdoc}
   */
  public function save($entity) {
    $now = REQUEST_TIME;
    if (empty($entity->created)) {
      $entity->created = $now;
    }
    $entity->changed = $now;
    if (empty($entity->uid)) {
      global $user;
      $entity->uid = $user->uid;
    }
    return parent::save($entity);
  }

}

// ---------------------------------------------------------------------------
// Revenue Line
// ---------------------------------------------------------------------------

/**
 * Entity class for a Drop Finance revenue line.
 */
class DropRevenue extends Entity {

  /** @var int Revenue line ID. */
  public $rid;

  /** @var string Label. */
  public $label = '';

  /** @var float Amount per occurrence. */
  public $amount = 0;

  /** @var string Frequency: monthly, quarterly, annual, once. */
  public $frequency = 'monthly';

  /** @var int|null Linked CRM org ID. */
  public $cid = NULL;

  /** @var int|null Linked project ID. */
  public $pid = NULL;

  /** @var int Start date timestamp. */
  public $start_date = 0;

  /** @var int|null End date timestamp or NULL for ongoing. */
  public $end_date = NULL;

  /** @var int Active flag. */
  public $active = 1;

  /** @var string Optional notes. */
  public $notes = '';

  /** @var int Owner's uid. */
  public $uid = 0;

  /** @var int Created timestamp. */
  public $created = 0;

  /** @var int Changed timestamp. */
  public $changed = 0;

  /**
   * {@inheritdoc}
   */
  public function id() {
    return $this->rid;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'drop_revenue';
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultLabel() {
    return $this->label;
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/finance/revenue/' . $this->rid . '/edit', 'options' => array());
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultUri() {
    return array('path' => 'drop/finance/revenue/' . $this->rid . '/edit', 'options' => array());
  }

}

/**
 * Controller class for DropRevenue entities.
 */
class DropRevenueController extends EntityDatabaseStorageController {

  /**
   * {@inheritdoc}
   */
  public function save($entity) {
    $now = REQUEST_TIME;
    if (empty($entity->created)) {
      $entity->created = $now;
    }
    $entity->changed = $now;
    if (empty($entity->uid)) {
      global $user;
      $entity->uid = $user->uid;
    }
    return parent::save($entity);
  }

}

// ---------------------------------------------------------------------------
// Expense Line
// ---------------------------------------------------------------------------

/**
 * Entity class for a Drop Finance expense line.
 */
class DropExpense extends Entity {

  /** @var int Expense line ID. */
  public $eid;

  /** @var string Label. */
  public $label = '';

  /** @var float Amount per occurrence. */
  public $amount = 0;

  /** @var string Frequency: monthly, quarterly, annual, once. */
  public $frequency = 'monthly';

  /** @var int|null Linked vendor ID. */
  public $vid = NULL;

  /** @var string Category label for non-vendor expenses. */
  public $category = '';

  /** @var int Start date timestamp. */
  public $start_date = 0;

  /** @var int|null End date timestamp or NULL for ongoing. */
  public $end_date = NULL;

  /** @var int Active flag. */
  public $active = 1;

  /** @var string Optional notes. */
  public $notes = '';

  /** @var int Owner's uid. */
  public $uid = 0;

  /** @var int Created timestamp. */
  public $created = 0;

  /** @var int Changed timestamp. */
  public $changed = 0;

  /**
   * {@inheritdoc}
   */
  public function id() {
    return $this->eid;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'drop_expense';
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultLabel() {
    return $this->label;
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/finance/expenses/' . $this->eid . '/edit', 'options' => array());
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultUri() {
    return array('path' => 'drop/finance/expenses/' . $this->eid . '/edit', 'options' => array());
  }

}

/**
 * Controller class for DropExpense entities.
 */
class DropExpenseController extends EntityDatabaseStorageController {

  /**
   * {@inheritdoc}
   */
  public function save($entity) {
    $now = REQUEST_TIME;
    if (empty($entity->created)) {
      $entity->created = $now;
    }
    $entity->changed = $now;
    if (empty($entity->uid)) {
      global $user;
      $entity->uid = $user->uid;
    }
    return parent::save($entity);
  }

}

// ---------------------------------------------------------------------------
// Transaction
// ---------------------------------------------------------------------------

/**
 * Entity class for a Drop Finance bank transaction.
 */
class DropTransaction extends Entity {

  /** @var int Transaction ID. */
  public $tid;

  /** @var string SHA-256 hash for duplicate detection. */
  public $hash = '';

  /** @var int Transaction date timestamp. */
  public $date = 0;

  /** @var string Raw bank description. */
  public $description = '';

  /** @var float Amount (always positive). */
  public $amount = 0;

  /** @var string Type: debit or credit. */
  public $type = 'debit';

  /** @var string Cleaned vendor name. */
  public $vendor_name = '';

  /** @var int|null Linked vendor entity ID. */
  public $vid = NULL;

  /** @var string Category. */
  public $category = '';

  /** @var int 1 = personal expense. */
  public $is_personal = 0;

  /** @var string Check number. */
  public $check_number = '';

  /** @var float|null Running balance from CSV. */
  public $balance = NULL;

  /** @var string Optional notes. */
  public $notes = '';

  /** @var int Owner's uid. */
  public $uid = 0;

  /** @var int Created timestamp. */
  public $created = 0;

  /** @var int Changed timestamp. */
  public $changed = 0;

  /**
   * {@inheritdoc}
   */
  public function id() {
    return $this->tid;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'drop_transaction';
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultLabel() {
    return $this->description;
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/finance/transactions/' . $this->tid . '/edit', 'options' => array());
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultUri() {
    return array('path' => 'drop/finance/transactions/' . $this->tid . '/edit', 'options' => array());
  }

}

/**
 * Controller class for DropTransaction entities.
 */
class DropTransactionController extends EntityDatabaseStorageController {

  /**
   * {@inheritdoc}
   */
  public function save($entity) {
    $now = REQUEST_TIME;
    if (empty($entity->created)) {
      $entity->created = $now;
    }
    $entity->changed = $now;
    if (empty($entity->uid)) {
      global $user;
      $entity->uid = $user->uid;
    }
    return parent::save($entity);
  }

}
