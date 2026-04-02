<?php
/**
 * @file
 * Entity classes for Drop Strategic.
 */

/**
 * Entity class for a strategic goal.
 */
class DropGoalStrategic extends Entity {
  public $gid                   = NULL;
  public $title                 = '';
  public $type                  = 'team';
  public $owner_uid             = 0;
  public $status                = 'active';
  public $horizon               = 'quarterly';
  public $due_date              = NULL;
  public $body                  = '';
  public $body_format           = 'filtered_html';
  public $progress              = 0;
  public $private_notes         = '';
  public $private_notes_format  = 'filtered_html';
  public $uid                   = 0;
  public $created               = 0;
  public $changed               = 0;

  /** {@inheritdoc} */
  public function id() {
    return $this->gid;
  }

  /** {@inheritdoc} */
  public function entityType() {
    return 'drop_goal_strategic';
  }

  /** {@inheritdoc} */
  protected function defaultLabel() {
    return $this->title;
  }

  /** {@inheritdoc} */
  public function uri() {
    return array('path' => 'drop/strategic/' . $this->gid, 'options' => array());
  }

  /** {@inheritdoc} */
  protected function defaultUri() {
    return $this->uri();
  }
}

/**
 * Controller for DropGoalStrategic entities.
 */
class DropGoalStrategicController extends EntityPlusController {

  /** {@inheritdoc} */
  public function save($entity, ?DatabaseTransaction $transaction = NULL) {
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
