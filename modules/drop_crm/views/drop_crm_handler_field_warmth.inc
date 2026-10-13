<?php
/**
 * @file
 * Contains the CRM warmth field handler.
 */

/**
 * Warmth field whose click sort ranks Hot, Warm, Cold, then unset.
 *
 * The stored values sort alphabetically as cold, hot, warm, which is not a
 * useful order, so click sorting uses drop_crm_warmth_rank_formula(). A
 * click sort replaces the view's own sorts, so on contact views it also sorts
 * by name to keep contacts with the same warmth in a stable order.
 *
 * @ingroup views_field_handlers
 */
class drop_crm_handler_field_warmth extends views_handler_field_field {

  /**
   * {@inheritdoc}
   */
  function click_sort($order) {
    if (empty($this->options['click_sort_column'])) {
      return;
    }
    $this->ensure_my_table();
    $column = $this->table_alias . '.' . _field_sql_storage_columnname($this->definition['field_name'], $this->options['click_sort_column']);
    $this->query->add_orderby(NULL, drop_crm_warmth_rank_formula($column), $order, $this->table_alias . '_warmth_rank');
    if ($this->view->base_table === 'crm_contact') {
      $this->query->add_orderby('crm_contact', 'name', 'ASC');
    }
  }

}
