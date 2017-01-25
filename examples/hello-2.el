;; This file is part of Elfuse.

;; Elfuse is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; Elfuse is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with Elfuse.  If not, see <http://www.gnu.org/licenses/>.

(require 'elfuse)

(defvar hello-2--file-list '("." ".."))

(elfuse-define-op readdir (path)
  (message "READDIR: %s" path)
  (unless (equal path "/")
    (signal 'elfuse-op-error elfuse-ENOENT))
  (apply 'vector hello-2--file-list))

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (cond ((equal path "/")
         (vector 'dir 0))
        ((member (file-name-nondirectory path) hello-2--file-list)
         (vector 'file 0))
        (t (signal 'elfuse-op-error elfuse-ENOENT))))

(elfuse-define-op create (path)
  (message "CREATE: %s" path)
  (add-to-list 'hello-2--file-list (file-name-nondirectory path))
  0)
