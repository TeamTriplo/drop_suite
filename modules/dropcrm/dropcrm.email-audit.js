(function ($) {
  'use strict';

  Backdrop.behaviors.dropcrm_email_audit = {
    attach: function (context, settings) {

      // Save email button.
      $('.audit-email-save', context).once('audit-email-save').on('click', function () {
        var $btn   = $(this);
        var $wrap  = $btn.closest('.audit-no-email');
        var $input = $wrap.find('.audit-email-input');
        var cid    = $input.data('cid');
        var token  = $input.data('token');
        var email  = $.trim($input.val());

        if (!email) { return; }

        $btn.prop('disabled', true).text(Backdrop.t('Saving…'));

        $.post(Backdrop.settings.basePath + 'admin/drop/crm/email-audit/add-email', {
          cid:   cid,
          email: email,
          token: token
        }, function (data) {
          if (data.success) {
            $wrap.replaceWith('<span class="audit-email">' + $('<span>').text(data.email).html() + '</span>');
            $btn.closest('tr').removeClass('audit-row-missing-email');
            // Enable newsletter checkboxes in this row now that email exists.
            $btn.closest('tr').find('.audit-nl-noemail').each(function () {
              var tid = $(this).closest('td').index() - 2; // rough index; replaced on reload
              $(this).replaceWith('<input type="checkbox" class="audit-nl-checkbox" data-email="' + data.email + '" data-tid="' + tid + '" data-token="' + token + '" />');
            });
          }
          else {
            alert(data.error || Backdrop.t('Could not save email.'));
            $btn.prop('disabled', false).text(Backdrop.t('Save'));
          }
        }, 'json').fail(function () {
          alert(Backdrop.t('Request failed.'));
          $btn.prop('disabled', false).text(Backdrop.t('Save'));
        });
      });

      // Allow Enter key in email input to trigger save.
      $('.audit-email-input', context).once('audit-email-input').on('keydown', function (e) {
        if (e.which === 13) {
          $(this).closest('.audit-no-email').find('.audit-email-save').trigger('click');
        }
      });

      // Newsletter checkbox toggle.
      $('.audit-nl-checkbox', context).once('audit-nl-checkbox').on('change', function () {
        var $cb    = $(this);
        var email  = $cb.data('email');
        var tid    = $cb.data('tid');
        var token  = $cb.data('token');
        var sub    = $cb.is(':checked') ? 1 : 0;

        $cb.prop('disabled', true);

        $.post(Backdrop.settings.basePath + 'admin/drop/crm/email-audit/toggle-newsletter', {
          email:     email,
          tid:       tid,
          subscribe: sub,
          token:     token
        }, function (data) {
          if (data.success) {
            $cb.prop('disabled', false);
            $cb.attr('title', sub ? Backdrop.t('Subscribed') : Backdrop.t('Not subscribed'));
          }
          else {
            alert(data.error || Backdrop.t('Could not update subscription.'));
            $cb.prop('checked', !sub).prop('disabled', false);
          }
        }, 'json').fail(function () {
          alert(Backdrop.t('Request failed.'));
          $cb.prop('checked', !sub).prop('disabled', false);
        });
      });

    }
  };

}(jQuery));
