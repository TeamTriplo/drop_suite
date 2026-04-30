/**
 * @file
 * Attaches a jQuery UI datepicker to Drop CRM date fields.
 */
(function ($) {
  Backdrop.behaviors.dropCrmDatepicker = {
    attach: function (context, settings) {
      $('.drop_crm-datepicker', context).once('drop_crm-datepicker').each(function () {
        var $input = $(this);
        $input.datepicker({
          dateFormat: 'yy-mm-dd',
          changeMonth: true,
          changeYear: true,
          yearRange: '-10:+5',
          onSelect: function (dateText) {
            $input.val(dateText);
          }
        });
      });
    }
  };
})(jQuery);
