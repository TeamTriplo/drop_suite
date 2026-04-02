/**
 * @file
 * Attaches a jQuery UI datepicker to DropTasks date fields.
 */
(function ($) {
  Backdrop.behaviors.dropTasksDatepicker = {
    attach: function (context, settings) {
      $('.droptasks-datepicker', context).once('droptasks-datepicker').each(function () {
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
