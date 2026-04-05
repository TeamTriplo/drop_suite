<?php
/**
 * @file
 * Entity classes for Drop Base: DropProject.
 */

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
    return array('path' => 'drop/projects/' . $this->pid, 'options' => array());
  }

  /** {@inheritdoc} */
  protected function defaultUri() {
    return $this->uri();
  }
}

/**
 * Controller for DropProject entities.
 */
class DropProjectController extends EntityPlusController {

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
