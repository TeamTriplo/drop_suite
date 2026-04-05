(function ($) {
  'use strict';

  /**
   * Kanban ticket modals — view/edit existing tickets and quick-add new ones.
   */
  Backdrop.behaviors.dropticketsModal = {
    attach: function (context, settings) {
      var board = context.querySelector('.droptickets-kanban');
      if (!board || board.dataset.modalInit) return;
      board.dataset.modalInit = 'true';

      var cfg       = settings.droptickets || {};
      var baseUrl   = cfg.kanbanStatusUrl || '';
      var token     = cfg.kanbanToken || '';
      var assignees = cfg.modalAssignees || [];
      var projects  = cfg.modalProjects || [];
      var types     = cfg.modalTypes || {};
      var priorities = cfg.modalPriorities || {};
      var quickAddUrl = cfg.modalQuickAddUrl || '';
      var tokenAssignee = cfg.tokenAssignee || '';
      var tokenPriority = cfg.tokenPriority || '';
      var boards    = cfg.modalBoards || [];

      // Build the modal DOM once.
      var overlay = _buildOverlay();
      document.body.appendChild(overlay);

      // Build the quick-add modal DOM once.
      var addOverlay = _buildAddOverlay();
      document.body.appendChild(addOverlay);

      // --- View/edit modal: open on card title click ---
      board.addEventListener('click', function (e) {
        var link = e.target.closest('.droptickets-modal-open');
        if (!link) return;
        e.preventDefault();
        var tid = link.dataset.tid;
        if (!tid) return;
        _openViewModal(tid);
      });

      // --- Quick-add: open on "+" button click ---
      board.addEventListener('click', function (e) {
        var btn = e.target.closest('.droptickets-kanban-quick-add');
        if (!btn) return;
        e.preventDefault();
        var status = btn.dataset.status;
        _openAddModal(status);
      });

      // =====================================================================
      // View/edit modal
      // =====================================================================

      function _openViewModal(tid) {
        var modal = overlay.querySelector('.droptickets-modal');
        modal.dataset.tid = tid;

        // Show loading state.
        overlay.querySelector('.droptickets-modal-title').textContent = Backdrop.t('Loading…');
        overlay.querySelector('.droptickets-modal-body-content').innerHTML = '';
        overlay.querySelector('.droptickets-modal-body-edit').style.display = 'none';
        overlay.querySelector('.droptickets-modal-body-view').style.display = 'block';
        overlay.querySelector('.droptickets-modal-sidebar').innerHTML = '';
        overlay.querySelector('.droptickets-modal-footer').innerHTML = '';
        overlay.classList.add('is-open');
        document.body.style.overflow = 'hidden';

        // Fetch data.
        var xhr = new XMLHttpRequest();
        xhr.open('GET', baseUrl + tid + '/modal-data', true);
        xhr.onload = function () {
          try {
            var data = JSON.parse(xhr.responseText);
            if (!data.ok) return;
            _renderViewModal(data);
          }
          catch (err) { /* ignore */ }
        };
        xhr.send();
      }

      function _renderViewModal(data) {
        var modal = overlay.querySelector('.droptickets-modal');

        // Title.
        overlay.querySelector('.droptickets-modal-label').textContent = data.label;
        overlay.querySelector('.droptickets-modal-title').textContent = data.title;

        // Body.
        var bodyView = overlay.querySelector('.droptickets-modal-body-view');
        var bodyContent = overlay.querySelector('.droptickets-modal-body-content');
        var bodyEdit = overlay.querySelector('.droptickets-modal-body-edit');
        var bodyTextarea = overlay.querySelector('.droptickets-modal-body-textarea');

        if (data.body_html) {
          bodyContent.innerHTML = data.body_html;
        }
        else {
          bodyContent.innerHTML = '<p class="droptickets-modal-empty">' + Backdrop.t('No description.') + '</p>';
        }
        bodyView.style.display = 'block';
        bodyEdit.style.display = 'none';
        bodyTextarea.value = data.body_raw || '';

        // Edit button for description.
        var editBtn = overlay.querySelector('.droptickets-modal-body-edit-btn');
        editBtn.style.display = data.can_edit ? '' : 'none';

        // Sidebar: assignee + priority selects (or labels if read-only).
        var sidebar = overlay.querySelector('.droptickets-modal-sidebar');
        sidebar.innerHTML = '';

        if (data.can_edit) {
          // Assignee select.
          sidebar.innerHTML += '<div class="droptickets-modal-field">'
            + '<label>' + Backdrop.t('Assignee') + '</label>'
            + _buildAssigneeSelect(data.assignee, 'droptickets-modal-assignee-select')
            + '</div>';

          // Priority select.
          sidebar.innerHTML += '<div class="droptickets-modal-field">'
            + '<label>' + Backdrop.t('Priority') + '</label>'
            + _buildPrioritySelect(data.priority, 'droptickets-modal-priority-select')
            + '</div>';

          // Boards checkboxes.
          if (boards.length > 0) {
            sidebar.innerHTML += '<div class="droptickets-modal-field">'
              + '<label>' + Backdrop.t('Boards') + '</label>'
              + _buildBoardCheckboxes(data.boards || [], 'droptickets-modal-boards')
              + '</div>';
          }

          // Bind change events.
          var assigneeSel = sidebar.querySelector('.droptickets-modal-assignee-select');
          assigneeSel.addEventListener('change', function () {
            _postInline(baseUrl + data.tid + '/inline-assignee', 'assignee', this.value, tokenAssignee);
          });
          var prioritySel = sidebar.querySelector('.droptickets-modal-priority-select');
          prioritySel.addEventListener('change', function () {
            _postInline(baseUrl + data.tid + '/inline-priority', 'priority', this.value, tokenPriority);
          });

          // Board checkbox changes.
          var boardContainer = sidebar.querySelector('.droptickets-modal-boards');
          if (boardContainer) {
            boardContainer.addEventListener('change', function () {
              var checked = boardContainer.querySelectorAll('input:checked');
              var vals = [];
              checked.forEach(function (cb) { vals.push(cb.value); });
              _postInline(baseUrl + data.tid + '/inline-boards', 'boards', vals.join(','), token);
            });
          }
        }
        else {
          var assigneeLabel = _assigneeLabel(data.assignee);
          var prioLabel = priorities[data.priority] || '—';
          sidebar.innerHTML += '<div class="droptickets-modal-field"><label>' + Backdrop.t('Assignee') + '</label><span>' + Backdrop.checkPlain(assigneeLabel) + '</span></div>';
          sidebar.innerHTML += '<div class="droptickets-modal-field"><label>' + Backdrop.t('Priority') + '</label><span>' + Backdrop.checkPlain(prioLabel) + '</span></div>';
          // Boards (read-only).
          if (boards.length > 0 && data.boards && data.boards.length > 0) {
            var boardNames = [];
            for (var b = 0; b < boards.length; b++) {
              if (data.boards.indexOf(boards[b].value) !== -1) {
                boardNames.push(Backdrop.checkPlain(boards[b].label));
              }
            }
            if (boardNames.length > 0) {
              sidebar.innerHTML += '<div class="droptickets-modal-field"><label>' + Backdrop.t('Boards') + '</label><span>' + boardNames.join(', ') + '</span></div>';
            }
          }
        }

        // Footer: comment count + link to full ticket.
        var footer = overlay.querySelector('.droptickets-modal-footer');
        var commentIcon = '<span class="droptickets-modal-comments">'
          + Backdrop.t('@count comments', {'@count': data.comment_count})
          + '</span>';
        footer.innerHTML = commentIcon
          + ' <a href="' + data.view_url + '" class="droptickets-modal-view-link">'
          + Backdrop.t('View full ticket') + ' →</a>';
      }

      // --- Description editing ---

      overlay.addEventListener('click', function (e) {
        // Edit button.
        if (e.target.closest('.droptickets-modal-body-edit-btn')) {
          overlay.querySelector('.droptickets-modal-body-view').style.display = 'none';
          overlay.querySelector('.droptickets-modal-body-edit').style.display = 'block';
          overlay.querySelector('.droptickets-modal-body-textarea').focus();
          return;
        }
        // Cancel edit.
        if (e.target.closest('.droptickets-modal-body-cancel')) {
          overlay.querySelector('.droptickets-modal-body-edit').style.display = 'none';
          overlay.querySelector('.droptickets-modal-body-view').style.display = 'block';
          return;
        }
        // Save edit.
        if (e.target.closest('.droptickets-modal-body-save')) {
          var tid = overlay.querySelector('.droptickets-modal').dataset.tid;
          var body = overlay.querySelector('.droptickets-modal-body-textarea').value;
          var saveBtn = overlay.querySelector('.droptickets-modal-body-save');
          saveBtn.disabled = true;
          saveBtn.textContent = Backdrop.t('Saving…');

          var xhr = new XMLHttpRequest();
          xhr.open('POST', baseUrl + tid + '/inline-body', true);
          xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
          xhr.onload = function () {
            saveBtn.disabled = false;
            saveBtn.textContent = Backdrop.t('Save');
            try {
              var resp = JSON.parse(xhr.responseText);
              if (resp.ok) {
                var bodyContent = overlay.querySelector('.droptickets-modal-body-content');
                if (resp.body_html) {
                  bodyContent.innerHTML = resp.body_html;
                }
                else {
                  bodyContent.innerHTML = '<p class="droptickets-modal-empty">' + Backdrop.t('No description.') + '</p>';
                }
                overlay.querySelector('.droptickets-modal-body-edit').style.display = 'none';
                overlay.querySelector('.droptickets-modal-body-view').style.display = 'block';
              }
            }
            catch (err) { /* ignore */ }
          };
          xhr.send('body=' + encodeURIComponent(body) + '&token=' + encodeURIComponent(token));
          return;
        }
      });

      // --- Close view modal ---
      overlay.addEventListener('click', function (e) {
        if (e.target === overlay || e.target.closest('.droptickets-modal-close')) {
          _closeViewModal();
        }
      });
      overlay.addEventListener('keydown', function (e) {
        if (e.key === 'Escape') _closeViewModal();
      });

      function _closeViewModal() {
        overlay.classList.remove('is-open');
        document.body.style.overflow = '';
      }

      // =====================================================================
      // Quick-add modal
      // =====================================================================

      function _openAddModal(status) {
        addOverlay.querySelector('.droptickets-add-status').value = status;
        addOverlay.querySelector('.droptickets-add-title').value = '';
        addOverlay.querySelector('.droptickets-add-body').value = '';
        addOverlay.querySelectorAll('.droptickets-add-boards input').forEach(function (cb) { cb.checked = false; });
        addOverlay.querySelector('.droptickets-add-error').textContent = '';
        addOverlay.classList.add('is-open');
        document.body.style.overflow = 'hidden';
        // Focus the title field after a short delay for the modal to render.
        setTimeout(function () {
          addOverlay.querySelector('.droptickets-add-title').focus();
        }, 50);
      }

      // Submit quick-add.
      addOverlay.addEventListener('click', function (e) {
        if (e.target.closest('.droptickets-add-submit')) {
          _submitQuickAdd();
        }
      });

      // Submit on Enter in the title field.
      addOverlay.addEventListener('keydown', function (e) {
        if (e.key === 'Enter' && e.target.classList.contains('droptickets-add-title')) {
          e.preventDefault();
          _submitQuickAdd();
        }
      });

      function _submitQuickAdd() {
        var title    = addOverlay.querySelector('.droptickets-add-title').value.trim();
        var body     = addOverlay.querySelector('.droptickets-add-body').value;
        var status   = addOverlay.querySelector('.droptickets-add-status').value;
        var pid      = addOverlay.querySelector('.droptickets-add-project').value;
        var type     = addOverlay.querySelector('.droptickets-add-type').value;
        var assignee = addOverlay.querySelector('.droptickets-add-assignee').value;
        var priority = addOverlay.querySelector('.droptickets-add-priority').value;
        var boardChecks = addOverlay.querySelectorAll('.droptickets-add-boards input:checked');
        var boardVals = [];
        boardChecks.forEach(function (cb) { boardVals.push(cb.value); });
        var errorEl  = addOverlay.querySelector('.droptickets-add-error');

        if (!title) {
          errorEl.textContent = Backdrop.t('Title is required.');
          return;
        }
        errorEl.textContent = '';

        var submitBtn = addOverlay.querySelector('.droptickets-add-submit');
        submitBtn.disabled = true;
        submitBtn.textContent = Backdrop.t('Creating…');

        var params = 'title=' + encodeURIComponent(title)
          + '&body=' + encodeURIComponent(body)
          + '&status=' + encodeURIComponent(status)
          + '&pid=' + encodeURIComponent(pid)
          + '&type=' + encodeURIComponent(type)
          + '&assignee=' + encodeURIComponent(assignee)
          + '&priority=' + encodeURIComponent(priority)
          + '&boards=' + encodeURIComponent(boardVals.join(','))
          + '&token=' + encodeURIComponent(token);

        var xhr = new XMLHttpRequest();
        xhr.open('POST', quickAddUrl, true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.onload = function () {
          submitBtn.disabled = false;
          submitBtn.textContent = Backdrop.t('Create ticket');
          try {
            var resp = JSON.parse(xhr.responseText);
            if (resp.ok) {
              _insertCard(resp, status);
              _closeAddModal();
            }
            else {
              errorEl.textContent = resp.message || Backdrop.t('Error creating ticket.');
            }
          }
          catch (err) {
            errorEl.textContent = Backdrop.t('Error creating ticket.');
          }
        };
        xhr.onerror = function () {
          submitBtn.disabled = false;
          submitBtn.textContent = Backdrop.t('Create ticket');
          errorEl.textContent = Backdrop.t('Network error.');
        };
        xhr.send(params);
      }

      function _insertCard(resp, status) {
        var col = board.querySelector('.droptickets-kanban-col[data-status="' + status + '"]');
        if (!col) return;

        var cards = col.querySelector('.droptickets-kanban-cards');
        var empty = cards.querySelector('.droptickets-kanban-empty');
        if (empty) empty.remove();

        var priorityClass = 'drop-priority-' + _priorityClass(resp.priority);
        var card = document.createElement('div');
        card.className = 'droptickets-kanban-card ' + priorityClass;
        card.draggable = true;
        card.dataset.tid = resp.tid;

        card.innerHTML = '<div class="droptickets-kanban-card-id"><span class="droptickets-ticket-id">' + Backdrop.checkPlain(resp.label) + '</span></div>'
          + '<div class="droptickets-kanban-card-title"><a href="' + resp.view_url + '" class="droptickets-modal-open" data-tid="' + resp.tid + '">' + Backdrop.checkPlain(resp.title) + '</a></div>'
          + '<div class="droptickets-kanban-card-meta">'
          + '<span class="droptickets-type droptickets-type--' + Backdrop.checkPlain(resp.type) + '">' + Backdrop.checkPlain(resp.type_label) + '</span>'
          + '<span class="droptickets-kanban-assignee">' + Backdrop.checkPlain(resp.assignee_name) + '</span>'
          + '</div>';

        cards.appendChild(card);

        // Update column count.
        var counter = col.querySelector('.droptickets-kanban-col-count');
        if (counter) {
          var n = col.querySelectorAll('.droptickets-kanban-card').length;
          counter.textContent = n;
        }
      }

      // Close quick-add modal.
      addOverlay.addEventListener('click', function (e) {
        if (e.target === addOverlay || e.target.closest('.droptickets-modal-close')) {
          _closeAddModal();
        }
      });
      addOverlay.addEventListener('keydown', function (e) {
        if (e.key === 'Escape') _closeAddModal();
      });

      function _closeAddModal() {
        addOverlay.classList.remove('is-open');
        document.body.style.overflow = '';
      }

      // =====================================================================
      // Shared helpers
      // =====================================================================

      function _buildOverlay() {
        var el = document.createElement('div');
        el.className = 'droptickets-modal-overlay';
        el.setAttribute('role', 'dialog');
        el.setAttribute('aria-modal', 'true');
        el.setAttribute('tabindex', '-1');
        el.innerHTML = '<div class="droptickets-modal">'
          + '<div class="droptickets-modal-header">'
          + '<div class="droptickets-modal-header-left">'
          + '<span class="droptickets-modal-label"></span>'
          + '<h2 class="droptickets-modal-title"></h2>'
          + '</div>'
          + '<button type="button" class="droptickets-modal-close" aria-label="' + Backdrop.t('Close') + '">&times;</button>'
          + '</div>'
          + '<div class="droptickets-modal-content">'
          + '<div class="droptickets-modal-main">'
          + '<div class="droptickets-modal-body-view">'
          + '<div class="droptickets-modal-body-header">'
          + '<h3>' + Backdrop.t('Description') + '</h3>'
          + '<button type="button" class="droptickets-modal-body-edit-btn">' + Backdrop.t('Edit') + '</button>'
          + '</div>'
          + '<div class="droptickets-modal-body-content"></div>'
          + '</div>'
          + '<div class="droptickets-modal-body-edit" style="display:none">'
          + '<textarea class="droptickets-modal-body-textarea" rows="10"></textarea>'
          + '<div class="droptickets-modal-body-actions">'
          + '<button type="button" class="droptickets-modal-body-save">' + Backdrop.t('Save') + '</button>'
          + '<button type="button" class="droptickets-modal-body-cancel">' + Backdrop.t('Cancel') + '</button>'
          + '</div>'
          + '</div>'
          + '</div>'
          + '<div class="droptickets-modal-sidebar"></div>'
          + '</div>'
          + '<div class="droptickets-modal-footer"></div>'
          + '</div>';
        return el;
      }

      function _buildAddOverlay() {
        var el = document.createElement('div');
        el.className = 'droptickets-modal-overlay droptickets-add-overlay';
        el.setAttribute('role', 'dialog');
        el.setAttribute('aria-modal', 'true');
        el.setAttribute('tabindex', '-1');

        var projectOpts = '<option value="0">' + Backdrop.t('— No project —') + '</option>';
        for (var i = 0; i < projects.length; i++) {
          projectOpts += '<option value="' + Backdrop.checkPlain(projects[i].value) + '">' + Backdrop.checkPlain(projects[i].label) + '</option>';
        }

        var typeOpts = '';
        for (var tkey in types) {
          if (types.hasOwnProperty(tkey)) {
            var sel = tkey === 'task' ? ' selected' : '';
            typeOpts += '<option value="' + Backdrop.checkPlain(tkey) + '"' + sel + '>' + Backdrop.checkPlain(types[tkey]) + '</option>';
          }
        }

        el.innerHTML = '<div class="droptickets-modal droptickets-modal--add">'
          + '<div class="droptickets-modal-header">'
          + '<h2 class="droptickets-modal-title">' + Backdrop.t('Quick add ticket') + '</h2>'
          + '<button type="button" class="droptickets-modal-close" aria-label="' + Backdrop.t('Close') + '">&times;</button>'
          + '</div>'
          + '<div class="droptickets-modal-content droptickets-add-content">'
          + '<input type="hidden" class="droptickets-add-status" value="">'
          + '<div class="droptickets-modal-field">'
          + '<label>' + Backdrop.t('Title') + ' <span class="form-required">*</span></label>'
          + '<input type="text" class="droptickets-add-title" maxlength="255" placeholder="' + Backdrop.t('Ticket title…') + '">'
          + '</div>'
          + '<div class="droptickets-modal-field">'
          + '<label>' + Backdrop.t('Description') + '</label>'
          + '<textarea class="droptickets-add-body" rows="4" placeholder="' + Backdrop.t('Optional description…') + '"></textarea>'
          + '</div>'
          + '<div class="droptickets-add-row">'
          + '<div class="droptickets-modal-field droptickets-add-field-half">'
          + '<label>' + Backdrop.t('Project') + '</label>'
          + '<select class="droptickets-add-project">' + projectOpts + '</select>'
          + '</div>'
          + '<div class="droptickets-modal-field droptickets-add-field-half">'
          + '<label>' + Backdrop.t('Type') + '</label>'
          + '<select class="droptickets-add-type">' + typeOpts + '</select>'
          + '</div>'
          + '</div>'
          + '<div class="droptickets-add-row">'
          + '<div class="droptickets-modal-field droptickets-add-field-half">'
          + '<label>' + Backdrop.t('Assignee') + '</label>'
          + _buildAssigneeSelect('', 'droptickets-add-assignee')
          + '</div>'
          + '<div class="droptickets-modal-field droptickets-add-field-half">'
          + '<label>' + Backdrop.t('Priority') + '</label>'
          + _buildPrioritySelect(1, 'droptickets-add-priority')
          + '</div>'
          + '</div>'
          + (boards.length > 0
            ? '<div class="droptickets-modal-field">'
              + '<label>' + Backdrop.t('Boards') + '</label>'
              + _buildBoardCheckboxes([], 'droptickets-add-boards')
              + '</div>'
            : '')
          + '<div class="droptickets-add-error"></div>'
          + '</div>'
          + '<div class="droptickets-modal-footer droptickets-add-footer">'
          + '<button type="button" class="droptickets-add-submit">' + Backdrop.t('Create ticket') + '</button>'
          + '</div>'
          + '</div>';

        return el;
      }

      function _buildAssigneeSelect(currentVal, className) {
        var html = '<select class="' + className + '">';
        html += '<option value=""' + (currentVal === '' ? ' selected' : '') + '>' + Backdrop.t('— Unassigned —') + '</option>';
        html += '<option value="team"' + (currentVal === 'team' ? ' selected' : '') + '>' + Backdrop.t('The Team') + '</option>';
        html += '<option value="on_hold"' + (currentVal === 'on_hold' ? ' selected' : '') + '>' + Backdrop.t('On Hold') + '</option>';
        for (var i = 0; i < assignees.length; i++) {
          var sel = (currentVal === assignees[i].value) ? ' selected' : '';
          html += '<option value="' + Backdrop.checkPlain(assignees[i].value) + '"' + sel + '>' + Backdrop.checkPlain(assignees[i].label) + '</option>';
        }
        html += '</select>';
        return html;
      }

      function _buildPrioritySelect(currentVal, className) {
        var html = '<select class="' + className + '">';
        for (var key in priorities) {
          if (priorities.hasOwnProperty(key)) {
            var sel = (parseInt(key) === parseInt(currentVal)) ? ' selected' : '';
            html += '<option value="' + key + '"' + sel + '>' + Backdrop.checkPlain(priorities[key]) + '</option>';
          }
        }
        html += '</select>';
        return html;
      }

      function _assigneeLabel(val) {
        if (val === 'team') return Backdrop.t('The Team');
        if (val === 'on_hold') return Backdrop.t('On Hold');
        if (val === '' || val === '0') return Backdrop.t('Unassigned');
        for (var i = 0; i < assignees.length; i++) {
          if (assignees[i].value === val) return assignees[i].label;
        }
        return '—';
      }

      function _buildBoardCheckboxes(currentVals, className) {
        var html = '<div class="' + className + '">';
        for (var i = 0; i < boards.length; i++) {
          var checked = (currentVals.indexOf(boards[i].value) !== -1) ? ' checked' : '';
          html += '<label class="droptickets-modal-board-label">'
            + '<input type="checkbox" value="' + Backdrop.checkPlain(boards[i].value) + '"' + checked + '> '
            + Backdrop.checkPlain(boards[i].label)
            + '</label>';
        }
        html += '</div>';
        return html;
      }

      function _postInline(url, fieldName, value, inlineToken) {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', url, true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send(fieldName + '=' + encodeURIComponent(value) + '&token=' + encodeURIComponent(inlineToken));
      }

      function _priorityClass(p) {
        var classes = {0: 'low', 1: 'normal', 2: 'high', 3: 'urgent', 4: 'critical'};
        return classes[p] || 'normal';
      }
    }
  };

}(jQuery));
