<?php
/**
 * @file
 * Contains the CRM warmth sort handler.
 */

/**
 * Sorts contacts by warmth rank: Hot, Warm, Cold, then unset.
 *
 * @ingroup views_sort_handlers
 */
class drop_crm_handler_sort_warmth extends views_handler_sort {

  /**
   * {@inheritdoc}
   */
  function query() {
    $this->ensure_my_table();
    $column = $this->table_alias . '.' . $this->real_field;
    $this->query->add_orderby(NULL, drop_crm_warmth_rank_formula($column), $this->options['order'], $this->table_alias . '_warmth_rank');
  }

}
