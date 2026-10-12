<?php
/**
 * @file
 * Contains the CRM warmth field handler.
 */

/**
 * Warmth field whose click sort ranks Hot, Warm, Cold, then unset.
 *
 * The stored values sort alphabetically as cold, hot, warm, which is not a
 * useful order, so click sorting uses drop_crm_warmth_rank_formula().
 *
 * @ingroup views_field_handlers
 */
class drop_crm_handler_field_warmth extends views_handler_field_field {

  /**
   * {@inheritdoc}
   */
  function click_sort($order) {
    $this->ensure_my_table();
    $column = $this->table_alias . '.' . $this->definition['field_name'] . '_value';
    $this->query->add_orderby(NULL, drop_crm_warmth_rank_formula($column), $order, $this->table_alias . '_warmth_rank');
  }

}
