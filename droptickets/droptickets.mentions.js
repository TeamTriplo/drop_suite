/**
 * @file
 * Attaches @mention autocomplete to DropTickets comment textareas.
 *
 * Requires jquery.textcomplete (bundled with the Mentions module) and the
 * drop_comment text format (which enables the Mentions filter).
 */
(function ($) {
  Backdrop.behaviors.dropticketsMentions = {
    attach: function (context, settings) {
      if (typeof $.fn.textcomplete === 'undefined') {
        return;
      }

      $('textarea.droptickets-comment-body', context).once('droptickets-mentions').textcomplete([{
        // Trigger on @ followed by word characters at end of typed string.
        match: /\B@(\w*)$/,

        search: function (term, callback) {
          $.getJSON(
            Backdrop.settings.basePath + 'mentions/autocomplete/drop_comment/' + encodeURIComponent(term),
            function (resp) { callback(resp); }
          );
        },

        // Insert @username followed by a space.
        replace: function (mention) {
          return '@' + mention + ' ';
        },

        index: 1
      }]);
    }
  };
})(jQuery);
